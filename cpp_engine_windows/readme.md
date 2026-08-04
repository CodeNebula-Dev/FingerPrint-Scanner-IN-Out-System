# Windows conversion 
To convert the macos version into Windows several changes are made inthe source code of the program. Which are as follows:-

## Two files are changed in the program .

###  toucd_id.mm -> windows_biometric.cpp
* This file in written in objective c++ ,the language which is absent in native Windows. This language is only used for development in macOS . So this file is completely removed from the source code and is replaced with a new file called windows_biometric.cpp.

* This new file windows_biometric.cpp contains the main logic for the accessing the integrated fingerprint scanner in the device. 

###  touch_id.h -> windows_biometric.h

* touch_id.h contains the core logic and the defination of the command which grants the permission for the integrated fingerprint scanner in the macos,This file also contained the defination of a command in the file touch_id.mm.
* Since touch_id.mm is changed to windows_biometrics.cpp , Thus the change is made in the respective header file i.e. windows_biometric.h .

### Some changes in the main.cpp

* Since the file main.cpp uses the command from the touch_id.h those files are changed to the windows_biometric.h. 

## How to run this file 

Write this command in the terminal
```
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release

```