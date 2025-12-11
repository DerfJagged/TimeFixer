# TimeFixer

TimeFixer is an application that allows you to set the date and time of an Xbox 360 to any value between the 11/15/2001 and 9/17/2036. This bypasses the software limitation of 2025 being the last year that can be set via the Microsoft Dashboard. This app is meant for consoles that are not connected to the internet, as the Microsoft Dashboard and Aurora dashboards will set the time via NTP if connected to the internet.

The Xbox 360's Real Time Clock (RTC) uses the same code as the original Xbox. The SMC maintains a 40-bit big-endian count of milliseconds since the Xbox epoch (`11-15-2001 00:00:00 UTC`), represented in hex. Once the value reaches `FFFFFFFFFF` (`09/17/2036 19:53:47 UTC`), it wraps back around to the epoch and starts the count over.

<img src="https://consolemods.org/wiki/images/e/e2/TimeFixer_Screenshot.png" width="49%" height="auto">

Big thanks to Visual Studio for helping figure out the RTC format encoding.
