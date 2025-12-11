//--------------------------------------------------------------------------------------
// TimeFixer.cpp
//
// Tool to set clock past 2025 while offline. 
//--------------------------------------------------------------------------------------
#include <xtl.h>
#include <AtgApp.h>
#include <AtgFont.h>
#include <AtgInput.h>
#include <AtgUtil.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "TimeFixer.h"

#define TICK_RATE 1000 // 1 tick = 1 ms
#define MAX_TICKS ((1ULL << 40) - 1ULL)
#define LEFT_THUMB_DEADZONE  16384 // -32768 - 32768
#define RIGHT_THUMB_DEADZONE 16384

char print_buffer[32];
wchar_t wide_print_buffer[32];
int selected_field = 0;
struct tm temp_tm;
struct tm xbox_epoch_tm = {0};
struct tm xbox_anti_epoch_tm = {0};
bool show_text = false;
float text_start_time = 0.0f; // in seconds
float text_duration   = 5.0f; // 5 seconds
wchar_t status_message[255];

class Lightshow : public ATG::Application {
    ATG::Font m_Font16; // 16-point font class
    ATG::Font m_Font12; // 12-point font class
	ATG::GAMEPAD* m_pGamepad;
    HRESULT DrawTextContent();

private:
    virtual HRESULT Initialize();
    virtual HRESULT Update();
    virtual HRESULT Render();
};

VOID __cdecl main() {
    Lightshow atgApp;
    ATG::GetVideoSettings( &atgApp.m_d3dpp.BackBufferWidth, &atgApp.m_d3dpp.BackBufferHeight );
    atgApp.m_d3dpp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    atgApp.Run();
}

HRESULT Lightshow::Initialize() {
    if( FAILED( m_Font16.Create( "game:\\Media\\Fonts\\Arial_16.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;
    if( FAILED( m_Font12.Create( "game:\\Media\\Fonts\\Arial_12.xpr" ) ) )
        return ATGAPPERR_MEDIANOTFOUND;

    m_Font12.SetWindow( ATG::GetTitleSafeArea() );
    m_Font16.SetWindow( ATG::GetTitleSafeArea() );
	
	// 00:00:00 11/15/2001
	xbox_epoch_tm.tm_year = 2001 - 1900; // tm is years since 1900
    xbox_epoch_tm.tm_mon  = 10;          // November = month 10
    xbox_epoch_tm.tm_mday = 15;          // 15th
	
	// 19:53:47 09/17/2036
	xbox_anti_epoch_tm.tm_year = 2036 - 1900; // tm is years since 1900
    xbox_anti_epoch_tm.tm_mon  = 8;           // September = month 9
    xbox_anti_epoch_tm.tm_mday = 17;          // 17th
	//xbox_anti_epoch_tm.tm_hour = 19;
	//xbox_anti_epoch_tm.tm_min  = 53;
	//xbox_anti_epoch_tm.tm_sec  = 46;

	GetRTCDate();

	// Load current date into editable field
	time_t decoded_rtc = decode_smc_rtc(m_SMCEditable);
    gmtime_s(&temp_tm, &decoded_rtc);
    
    return S_OK;
}

HRESULT Lightshow::Update() {
    // Get the current gamepad status
    m_pGamepad = ATG::Input::GetMergedInput();

	// Update selected selected_field (0-5); MM/DD/YYYY HH:mm:ss
	if (m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_LEFT || m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_LEFT_SHOULDER ) {
		selected_field--;
		if (selected_field < 0) {
			selected_field = 5;
		}
	}
	else if (m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_RIGHT || m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER ) {
		selected_field++;
	}

	selected_field = selected_field % 6;

	// Increment/decrement selected field; MM/DD/YYYY HH:mm:ss
	if (m_pGamepad->sThumbLY > LEFT_THUMB_DEADZONE || m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_UP || m_pGamepad->bRightTrigger) {
		switch(selected_field){
			case 0: //MM
				temp_tm.tm_mon++;
				break;
			case 1: //DD
				temp_tm.tm_mday++;
				break;
			case 2: //YYYY
				temp_tm.tm_year++;
				break;
			case 3: //HH
				temp_tm.tm_hour++;
				break;
			case 4: //mm
				temp_tm.tm_min++;
				break;
			case 5: //ss
				temp_tm.tm_sec++;
				break;
			default:
				selected_field = 0;
				break;
		}
	}
	else if (m_pGamepad->sThumbLY < -LEFT_THUMB_DEADZONE || m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_DPAD_DOWN || m_pGamepad->bLeftTrigger ) {
		switch(selected_field){
			case 0: //MM
				temp_tm.tm_mon--;
				break;
			case 1: //DD
				temp_tm.tm_mday--;
				break;
			case 2: //YYYY
				temp_tm.tm_year--;
				break;
			case 3: //HH
				temp_tm.tm_hour--;
				break;
			case 4: //mm
				temp_tm.tm_min--;
				break;
			case 5: //ss
				temp_tm.tm_sec--;
				break;
			default:
				selected_field = 0;
				break;
		}
	}

	// Check that it's after the Xbox epoch - 00:00:00 11/15/2001
	if ( (temp_tm.tm_year < 101) || 
		 (temp_tm.tm_year == 101 && temp_tm.tm_mon < 10) ||
		 (temp_tm.tm_year == 101 && temp_tm.tm_mon == 10 && temp_tm.tm_mday < 15) ) {
		//printf("\nDate before Xbox epoch, resetting to epoch");
		temp_tm = xbox_epoch_tm;
	}

	// Check that it's before the Xbox anti-epoch - 19:53:47 09/17/2036
	if ( (temp_tm.tm_year > 136) || 
		 (temp_tm.tm_year == 136 && temp_tm.tm_mon > 8) ||
		 (temp_tm.tm_year == 136 && temp_tm.tm_mon == 8 && temp_tm.tm_mday >= 17) ) {
		//printf("\nDate after Xbox anti-epoch, resetting to anti-epoch");
		temp_tm = xbox_anti_epoch_tm;
		temp_tm.tm_mday--;
	}

	// Normalize overflow/underflows, 6:6:6 if error
	if (mktime(&temp_tm) == (time_t)-1) {
		temp_tm.tm_hour = 6;
		temp_tm.tm_min = 6;
		temp_tm.tm_sec = 6;
	}

	if( ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_A ) || ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_START ) ) {
		// Set to chosen time
		unsigned char encoded_bytes[8];
		// Convert temp_tm into bytes
		time_t edited_time = _mkgmtime(&temp_tm);
		encode_smc_rtc(edited_time, encoded_bytes);
		memcpy(m_SMCEditable, &encoded_bytes, sizeof(uint64_t));
		SetRTCDate();
		ShowTextForSeconds(L"Time has been set", 5);
	}
	else if( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_B ) {
		// Reboot
		HalReturnToFirmware(6);
	}
	/*
	else if( ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_X ) )  {
		// Set to test time
		uint64_t timestamp = 0x8500FEFFFFFF0100ULL; // Right before anti-epoch
		memcpy(m_SMCEditable, &timestamp, sizeof(uint64_t));
		SetRTCDate();
	}
	else if( ( m_pGamepad->wPressedButtons & XINPUT_GAMEPAD_Y ) ) {
		// Get current time
		GetRTCDate();
	}
	*/

	return S_OK;
}

HRESULT Lightshow::DrawTextContent() {
    D3DRECT rc = m_Font16.m_rcWindow;
	
	//// BIG TEXT
    m_Font16.Begin();
    m_Font16.DrawText( ( rc.x2 - rc.x1 ) / 2.0f, 50, 0xffffffff, L"Time Fixer", ATGFONT_CENTER_X );
	
	// Print current ticks to debug
	//printf("\nCurrent time:");
	//printf("%02X%02X%02X%02X%02X%02X%02X%02X\n", (unsigned char)m_SMCEditable[0], (unsigned char)m_SMCEditable[1], (unsigned char)m_SMCEditable[2], (unsigned char)m_SMCEditable[3], (unsigned char)m_SMCEditable[4], (unsigned char)m_SMCEditable[5], (unsigned char)m_SMCEditable[6], (unsigned char)m_SMCEditable[7]);
	
	// Print current ticks to screen
	//sprintf_s(print_buffer, sizeof(print_buffer), "%02X %02X %02X %02X %02X %02X %02X %02X", m_SMCEditable[0], m_SMCEditable[1], m_SMCEditable[2], m_SMCEditable[3], m_SMCEditable[4], m_SMCEditable[5], m_SMCEditable[6], m_SMCEditable[7]);
	//mbstowcs(wide_print_buffer, print_buffer, 32);
	//m_Font16.DrawText( ( rc.x2 - rc.x1 ) / 2.0f, 400, 0xffffffff, wide_print_buffer, ATGFONT_CENTER_X );

	// Print current MM:DD:YYYY HH:mm:ss to debug
	//time_t decoded_rtc = decode_smc_rtc(m_SMCEditable);
    //gmtime_s(&temp_tm, &decoded_rtc);
    //printf("Decoded UTC = %04d-%02d-%02d %02d:%02d:%02d\n",
    //       temp_tm.tm_year + 1900, temp_tm.tm_mon + 1, temp_tm.tm_mday,
    //       temp_tm.tm_hour, temp_tm.tm_min, temp_tm.tm_sec);
	
	// Print current MM:DD:YYYY HH:mm:ss to screen
	//sprintf_s(print_buffer, sizeof(print_buffer), "Decoded UTC = %04d-%02d-%02d %02d:%02d:%02d",
	//		temp_tm.tm_year + 1900, temp_tm.tm_mon + 1, temp_tm.tm_mday,
	//		temp_tm.tm_hour, temp_tm.tm_min, temp_tm.tm_sec);

	//Month (MM)
	sprintf_s(print_buffer, sizeof(print_buffer), "%02d", temp_tm.tm_mon + 1);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 150, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 135, 120, 0xffffffff, L"/", ATGFONT_RIGHT );

	//Day (DD)
	sprintf_s(print_buffer, sizeof(print_buffer), "%02d", temp_tm.tm_mday);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 105, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 90, 120, 0xffffffff, L"/", ATGFONT_RIGHT );

	//Year (YYYY)
	sprintf_s(print_buffer, sizeof(print_buffer), "%04d", temp_tm.tm_year + 1900);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 35, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );

	//Hour (HH)
	sprintf_s(print_buffer, sizeof(print_buffer), "%02d", temp_tm.tm_hour);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 50, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 60, 120, 0xffffffff, L":", ATGFONT_RIGHT );

	//Minute (mm)
	sprintf_s(print_buffer, sizeof(print_buffer), "%02d", temp_tm.tm_min);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 85, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 95, 120, 0xffffffff, L":", ATGFONT_RIGHT );

	//Second (SS)
	sprintf_s(print_buffer, sizeof(print_buffer), "%02d", temp_tm.tm_sec);
	mbstowcs(wide_print_buffer, print_buffer, 64);
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 120, 120, 0xffffffff, wide_print_buffer, ATGFONT_RIGHT );
	m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 175, 120, 0xffffffff, L"UTC", ATGFONT_RIGHT );

	//Underline selected field
	switch(selected_field){
        case 0: //MM
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 150, 125, 0xffffffff, L"__", ATGFONT_RIGHT );
			break;
		case 1: //DD
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 105, 125, 0xffffffff, L"__", ATGFONT_RIGHT );
			break;
		case 2: //YYYY
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) - 35, 125, 0xffffffff, L"____", ATGFONT_RIGHT );
			break;
		case 3: //HH
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 50, 125, 0xffffffff, L"__", ATGFONT_RIGHT );
			break;
		case 4: //mm
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 85, 125, 0xffffffff, L"__", ATGFONT_RIGHT );
			break;
		case 5: //ss
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f) + 120, 125, 0xffffffff, L"__", ATGFONT_RIGHT );
			break;
		default:
			selected_field = 0;
			break;
    }

	// Temporary message printed to screen
	if (show_text) {
		// Compute time elapsed since start of showing text
		LARGE_INTEGER freq, counter;
		QueryPerformanceFrequency(&freq);
		QueryPerformanceCounter(&counter);
		float now = (float)counter.QuadPart / (float)freq.QuadPart;
		float elapsed = now - text_start_time;

		// Hide after 5 seconds
		if (elapsed < text_duration) {
			m_Font16.DrawText( (( rc.x2 - rc.x1 ) / 2.0f), 350, 0xffffffff, status_message, ATGFONT_CENTER_X );
		}
		else {
			show_text = false;
		}
	}
	
    m_Font16.End();

	//// SMALL TEXT
    m_Font12.Begin();
	m_Font12.DrawText( ( rc.x2 - rc.x1 ) / 2.0f - 160, 200, 0xffffffff, L"DPAD Left / Right or LB / RB to choose field\nDPAD Up / Down or LT / RT to change time\nPress (A) to save the chosen time\nPress (B) to reboot\n\nReboot is required for change to take effect.", ATGFONT_LEFT );
	m_Font12.DrawText( rc.x1 - 200.0f, rc.y2 - 100.0f, 0xffffffff, L"Made by Derf\nConsoleMods.org", ATGFONT_LEFT );
    m_Font12.End();

    return S_OK;
}

HRESULT Lightshow::Render() {
    ATG::RenderBackground( 0x00000000, 0x00000000 ); //Black
    DrawTextContent();
	m_pd3dDevice->Present( NULL, NULL, NULL, NULL );
    return S_OK;
}

void PrepareBuffers() {
	ZeroMemory( m_SMCMessage, sizeof(m_SMCMessage) );
	ZeroMemory( m_SMCReturn, sizeof(m_SMCReturn) );
}

void GetRTCDate() {
    PrepareBuffers();
    m_SMCMessage[0] = 0x04; //read clock
	HalSendSMCMessage(m_SMCMessage, m_SMCReturn);
	/*
	printf("\nCurrent time:", m_SMCReturn);
	printf("%02X%02X%02X%02X%02X%02X%02X%02X\n", (unsigned char)m_SMCReturn[0], (unsigned char)m_SMCReturn[1], (unsigned char)m_SMCReturn[2], (unsigned char)m_SMCReturn[3], (unsigned char)m_SMCReturn[4], (unsigned char)m_SMCReturn[5], (unsigned char)m_SMCReturn[6], (unsigned char)m_SMCReturn[7]);
	for (int i=0; i<8; i++) {
		printf("m_SMCReturn[%d] = %02X\n", i, (unsigned char)m_SMCReturn[i]);
	}*/

	memcpy(&m_SMCEditable, m_SMCReturn, sizeof(m_SMCEditable));
}

void SetRTCDate() {
    PrepareBuffers();
	memcpy(m_SMCMessage, &m_SMCEditable, sizeof(m_SMCEditable));
	m_SMCMessage[0] = 0x85; //set clock

	//printf("\nAbout to send:");
	//printf("%02X%02X%02X%02X%02X%02X%02X%02X\n", (unsigned char)m_SMCMessage[0], (unsigned char)m_SMCMessage[1], (unsigned char)m_SMCMessage[2], (unsigned char)m_SMCMessage[3], (unsigned char)m_SMCMessage[4], (unsigned char)m_SMCMessage[5], (unsigned char)m_SMCMessage[6], (unsigned char)m_SMCMessage[7]);
	HalSendSMCMessage(m_SMCMessage, NULL);
	GetRTCDate();
}

int hex_to_bytes(const char *hex, unsigned char *out, size_t max_len) {
    size_t len = 0;

    while (*hex && hex[1]) {
        if (len >= max_len) {
            return -1;
		}
        
		char c1 = *hex++;
        char c2 = *hex++;
       
		if (!isxdigit(c1) || !isxdigit(c2)) {
            return -2;
		}

        unsigned char b = 
            ((isdigit(c1) ? c1 - '0' : (toupper(c1) - 'A' + 10)) << 4) |
             (isdigit(c2) ? c2 - '0' : (toupper(c2) - 'A' + 10));

        out[len++] = b;
    }
    return (int)len;
}

time_t decode_smc_rtc(const unsigned char *data) {
	uint64_t ticks =
        ((uint64_t)(unsigned)data[1]) |
        ((uint64_t)(unsigned)data[2] << 8)  |
        ((uint64_t)(unsigned)data[3] << 16) |
        ((uint64_t)(unsigned)data[4] << 24) |
        ((uint64_t)(unsigned)data[5] << 32);

    uint64_t seconds = ticks / TICK_RATE;
	//printf("Decoded seconds = %ld\n", (long)seconds);
    time_t xbox_epoch = _mkgmtime(&xbox_epoch_tm);
    return xbox_epoch + (time_t)seconds;
}

void encode_smc_rtc(time_t t, unsigned char out[7]) {
    time_t xbox_epoch = _mkgmtime(&xbox_epoch_tm);

    if (t < xbox_epoch) {
        //printf("Date is before Xbox epoch!\n");
        return;
    }

    uint64_t delta_sec = (uint64_t)(t - xbox_epoch);
    uint64_t ticks = delta_sec * TICK_RATE;

    if (ticks > MAX_TICKS) {
        //printf("Date exceeds 40-bit tick limit!\n");
        return;
    }

	// Output packet
    out[0] = 0x04;					// Command = query date
	out[1] = (ticks)       & 0xFF;  // Tick - Least significant byte
    out[2] = (ticks >> 8)  & 0xFF;  // Tick
	out[3] = (ticks >> 16) & 0xFF;  // Tick
	out[4] = (ticks >> 24) & 0xFF;  // Tick
	out[5] = (ticks >> 32) & 0xFF;  // Tick - Most significant byte
    out[6] = 0x01;					// In sync; should always be true
	out[7] = 0x00;					// Reserved?
}

void ShowTextForSeconds(const wchar_t* message, float seconds) {
    show_text = true;
	wcsncpy(status_message, message, 255);
	LARGE_INTEGER freq, counter;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);
    text_start_time = (float)counter.QuadPart / (float)freq.QuadPart;
	text_duration = seconds;
}