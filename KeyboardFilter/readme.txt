This code is a kernel mode keylogger that is writing its keylogs to C:\logs.txt

Its using the Windows function "IoAttachDevice" to attach another device onto the keyboard device ("\\Device\\KeyboardClass0"), then using the CompelationRoutine in order to get all returned keys from the keyboard.
