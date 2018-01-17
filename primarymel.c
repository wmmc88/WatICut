//Motor && Sensor Aliases for readability
const tMotor zMotor = motorA;
const tSensors zTouch = S1;
const tSensors spindleServo = S2;
const tSensors eStopTouch = S3;

//Speed Constants
const int Z_CAL_POWER_FAST = 100;
const int Z_CAL_POWER_SLOW = 80;
const int Z_MOTOR_POWER = 75;
const float Z_MAX_ENC_TRAVEL = .25;
const float ENCODER_PPR = 360;
const float REV_PER_INCH = 24;
const float Z_GEAR_RATIO = 24.0/36;

//Bluetooth Commands
const int EMERGENCY_SHUTDOWN = 0xEF;
const int COMPLETE = 0xEE;
const int NO_MESSAGE = 0x0;
const int SHUTDOWN = 0x1;
const int CALIBRATE = 0x2;
const int FILE_TRANSFER = 0x3;
const int START_JOB = 0x4;
const int SPINDLE_UP = 0x5;
const int SPINDLE_DOWN = 0x6;

//Prototypes
task eStop(); 
void initializeBluetooth();
void clearMessageQueue();
void calibrateXY();
void calibrateZ();
void liftSpindle(bool dir);
bool findFile(string &fileName, unsigned short &fileSize);
void sendFileBT(string &fileName, unsigned short &fileSize);
void executeJob();

bool connectPort(int port, const string& sName);
bool bWaitForBluetoothIdle();
bool bWaitForBluetoothIdle(int kWaitDuration);

//Written By: Melvin Wang
task main()
{
	nNxtExitClicks = 4;

	displayCenteredTextLine(0, "Initializing BT");
	initializeBluetooth();
	clearMessageQueue();
	startTask(eStop);

	displayCenteredTextLine(0, "Motor/Sensor Init");
	//motors one brake mode
	bFloatDuringInactiveMotorPWM = false;
	//motorReversed?
	bMotorReflected[zMotor] = true;
	SensorType[zTouch] = sensorTouch;

	calibrateXY();
	calibrateZ();


	bool finished = false;
	string fileName = "";
	unsigned short fileSize = 0;
	eraseDisplay();
	while(!finished)
	{
		if(!findFile(fileName, fileSize))
		{
			displayCenteredTextLine(1, "No Job Files.");
			displayCenteredTextLine(2, "Waiting to");
			displayCenteredTextLine(3, "Receive Files");
			displayCenteredTextLine(4, "From PC.");
			displayCenteredTextLine(5, "Press Grey");
			displayCenteredTextLine(6, "Button to");
			displayCenteredTextLine(7, "Shutdown!");
			if (nNxtButtonPressed == 0)
			{
				finished = true;
			}
		}
		else
		{
			displayCenteredTextLine(0, "Print Next Job?");
			displayCenteredTextLine(1, "File Name:");
			displayCenteredTextLine(2, "%s", fileName);
			displayCenteredTextLine(3, "File Size:");
			displayCenteredTextLine(4, "%d", fileSize);
			displayCenteredTextLine(5, "Orange to Continue");
			displayCenteredTextLine(6, "Right to Skip File");
			displayCenteredTextLine(7, "Grey to Shutdown");

			int buttonPressed = false;
			while (!buttonPressed)
			{
				switch (nNxtButtonPressed)
				{
				case 0:
					finished = true;
					buttonPressed = true;
					break;

				case 1:
					TFileIOResult fileIOResult;
					Delete(fileName, fileIOResult);
					buttonPressed = true;
					break;

				case 3:
					sendFileBT(fileName, fileSize);
					executeJob();
					buttonPressed = true;
					break;
				}
			}
			eraseDisplay();
		}
	}
	sendMessage(SHUTDOWN);
	powerOff();
}

//Written by: Melvin Wang
task eStop()
{
	SensorType[eStopTouch] = sensorTouch;

	while(SensorValue[eStopTouch]==0){}
	hogCPU();
	displayCenteredTextLine(4, "EMERGENCY_SHUTDOWN");
	sendMessage(EMERGENCY_SHUTDOWN);
	powerOff();
}

//Written by: Melvin Wang
void initializeBluetooth()
{
	wait1Msec(2000);
	btDisconnectAll();
	while(!connectPort(2, "Slave44")){}
	wait1Msec(2000);
}

//Written by: Melvin Wang
void clearMessageQueue()
{
	int temp;
	while (bQueuedMsgAvailable())
	{
		ClearMessage();
		temp = message;
	}
	ClearMessage();
}

//Written by: Melvin Wang
void calibrateXY()
{
	displayCenteredTextLine(0, "Sending Cali CMD");
	sendMessage(CALIBRATE);
	wait1Msec(50);
	displayCenteredTextLine(0, "Waiting on XY");
	//wait for response to indicate completion
	while(message != COMPLETE){}
	ClearMessage();
}

//Written by: Melvin Wang
void calibrateZ()
{
	displayCenteredTextLine(0, "Calibrating Z");
	motor[zMotor] = -Z_CAL_POWER_FAST;
	while(SensorValue[zTouch] == 0){}
	nMotorEncoder[zMotor] = 0;
	//lift one rotation
	motor[zMotor] = Z_CAL_POWER_FAST;
	while(fabs(nMotorEncoder[zMotor] * Z_GEAR_RATIO / ENCODER_PPR / REV_PER_INCH) < 0.3){}
	//calibrate slowly
	motor[zMotor] = -Z_CAL_POWER_SLOW;
	while(SensorValue[zTouch] == 0){}
	motor[zMotor] = 0;
	nMotorEncoder[zMotor] = 0;
	motor[zMotor] = Z_CAL_POWER_FAST;
	while(fabs(nMotorEncoder[zMotor] * Z_GEAR_RATIO / ENCODER_PPR / REV_PER_INCH) < 0.43);
	motor[motorA] = 0;
}

//Written by: Gabriel Mok
void liftSpindle(bool dir) //FALSE FLIPS DIRECTION (i.e FALSE LOWERS SPINDLE)
{
	displayCenteredTextLine(0, "Spindle Height Changing");
	nMotorEncoder[zMotor] = 0;
	motor[zMotor] = -Z_MOTOR_POWER *(1-2*dir);
	while(fabs(nMotorEncoder[zMotor]*Z_GEAR_RATIO/ENCODER_PPR / REV_PER_INCH) < Z_MAX_ENC_TRAVEL){}
	motor[zMotor] = 0;
}

//Written by: Melvin Wang
bool findFile(string &fileName, unsigned short &fileSize)
{
	displayCenteredTextLine(0, "Finding Files...");
	TFileHandle fileHandle;
	TFileIOResult fileIOResult;
	FindFirstFile(fileHandle, fileIOResult, "*.txt", fileName, fileSize);

	//ignores files greater than mas size
	if(fileIOResult == 0 && fileSize <= 65535)//fileIOResult is non-zero if no file is found
	{
		Close(fileHandle, fileIOResult);
		return true;
	}
	else
	{
		Close(fileHandle, fileIOResult);
		return false;
	}
}

//Written by: Melvin Wang
void sendFileBT(string &fileName, unsigned short &fileSize)
{
	eraseDisplay();
	displayCenteredTextLine(0, "Sending File");
	sendMessage(FILE_TRANSFER);
	wait1Msec(30);

	TFileHandle fileHandle;
	TFileIOResult fileIOResult;

	OpenRead(fileHandle, fileIOResult, fileName, fileSize);

	sendMessage(fileSize);
	wait1Msec(30);

	short twoCharacters[3] = {0, 0, 0};
	for (int fileIndex = 0; fileIndex<fileSize/6; fileIndex++)
	{
		for (int index = 0; index<3; index++)
		{
			ReadShort(fileHandle, fileIOResult, twoCharacters[index]);
		}
		sendMessageWithParm(twoCharacters[0], twoCharacters[1], twoCharacters[2]);
		wait1Msec(30);
	}
	//in case file is not multiple of three shorts (6bytes)
	if(fileSize%6!=0)
	{
		for(int remainder = 0; remainder < fileSize%6; remainder++)
		{
			byte character = 0;
			ReadByte(fileHandle, fileIOResult, character);
			sendMessage(character);
			wait1Msec(30);
		}
	}
	Close(fileHandle, fileIOResult);
	Delete(fileName, fileIOResult);
	playSound(soundBeepBeep);
}

//Written by: Gabriel Mok
void executeJob()
{
	time1[T1] = 0;
	sendMessage(START_JOB);
	bool printing = true;
	while(printing)
	{
		switch (message)
		{
		case NO_MESSAGE:
			displayCenteredTextLine(0, "Waiting...");
			break;

		case COMPLETE:
			displayCenteredTextLine(0, "Job Finished...");
			ClearMessage();
			printing = false;
			break;

		case SPINDLE_UP:
			displayCenteredTextLine(0, "Lifting Spindle");
			ClearMessage();
			liftSpindle(true);
			sendMessage(COMPLETE);
			break;

		case SPINDLE_DOWN:
			displayCenteredTextLine(0, "Lowering Spindle");
			ClearMessage();
			liftSpindle(false);
			sendMessage(COMPLETE);
			break;

		default:
			displayCenteredTextLine(0, "INVALID BT COMMAND!");
			break;
		}
	}
	long time = time1[T1];
	int mins = (time/1000)/60;
	int seconds = (time/1000)%60;
	displayString(1, "Your job took %d minutes and %d seconds.",mins,seconds);
	wait1Msec(10000);
}

////////////////////////////////////////////////////////////////////////////////
/*Original Code from ROBOT C Sample Program
Modified by: Melvin Wang*/
bool connectPort(int port, const string& sName)
{
	TFileIOResult nConnStatus;

	if (!bWaitForBluetoothIdle())
		return false;
	nConnStatus = btConnect(port, sName);
	if (nConnStatus != ioRsltInProgress)
	{
		// Connection failed!!!
		playSound(soundLowBuzz);
		return false;
	}

	bWaitForBluetoothIdle(15000);
	nConnStatus = nBluetoothCmdStatus;
	if (nBTCurrentStreamIndex < 0)
	{
		// Connection failed!!!
		playSound(soundLowBuzz);
		return false;
	}
	else
	{
		int nTemp;
		nTemp = nBTCurrentStreamIndex;
		if (nTemp < 0)
		{
			// Connection failed!!!
			playSound(soundLowBuzz);
			return false;
		}
	}
	return nBluetoothCmdStatus == ioRsltSuccess;
}

bool bWaitForBluetoothIdle()
{
	return bWaitForBluetoothIdle(3000);
}

bool bWaitForBluetoothIdle(int kWaitDuration)
{
	int nWait;

	ClearMessage();
	for (nWait = 0; nWait < kWaitDuration; nWait += 10)
	{
		if (!bBTBusy)
			return true;
		wait1Msec(10);
	}
	return false;  // Still Busy
}
