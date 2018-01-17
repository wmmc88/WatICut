//Motor && Sensor Aliases for readability
const tMotor yMotor = motorA;
const tMotor xMotor = motorC;
const tSensors xTouch = S1;
const tSensors yTouch = S2;

//Motor Constants
const int XY_CAL_POWER_FAST = 50;
const int XY_CAL_POWER_SLOW = 20;
const int ENCODER_PPR = 360;//pulses per revolution
const float PIXELS_PER_INCH = 100;
const float X_GEAR_RATIO = 22.0/40;
const float Y_GEAR_RATIO = 2.0/3;
const float X_GEAR_CIRCUMFERENCE = PI*16/25.4;
const float Y_GEAR_CIRCUMFERENCE = PI*16/25.4;
const float SPEED_CONSTANT = 0.5;////last change


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

//Job File Sentinels
const int END_OF_JOB = -2;
const int NEW_LINE = -1;

//Prototypes
task eStop();
void clearMessageQueue();
void calibrateXY();
void receiveFile();
void executeJob();
void getNextPoint(TFileHandle &fileHandle, TFileIOResult &fileIOResult, float &xPower, float &yPower, int &xDistance, int &yDistance);
bool validChar(char testChar);
void moveXYMotors(float xPower, float yPower, int xDistance, int yDistance);

//Written by: Melvin Wang
task main()
{
	btDisconnectAll();
	startTask (eStop);
	displayCenteredTextLine(0, "Motor/Sensor Init");
	//motors one brake mode
	bFloatDuringInactiveMotorPWM = false;
	//motorReversed?
	bMotorReflected[xMotor] = false;
	bMotorReflected[yMotor] = false;
	nSyncedMotors = synchAB;
	nSyncedTurnRatio = 100;
	SensorType[xTouch] = sensorTouch;
	SensorType[yTouch] = sensorTouch;

	bool finished = false;

	clearMessageQueue();
	while (!finished)
	{
		switch(message)
		{
		case NO_MESSAGE:
			displayCenteredTextLine(0, "Waiting...");
			break;

		case SHUTDOWN:
			ClearMessage();
			finished = true;
			break;

		case CALIBRATE:
			displayCenteredTextLine(0, "Calibrating...");
			ClearMessage();
			calibrateXY();
			break;

		case FILE_TRANSFER:
			displayCenteredTextLine(0, "File Transferring...");
			ClearMessage();
			receiveFile();
			break;

		case START_JOB:
			displayCenteredTextLine(0, "Job In Progress...");
			ClearMessage();
			executeJob();
			sendMessage(COMPLETE);
			wait1Msec(30);
			break;

		default:
			displayCenteredTextLine(0, "INVALID BT COMMAND!");
			clearMessageQueue();
			wait1Msec(15000);
			break;
		}
	}

	displayCenteredTextLine(0, "Shutting Down");
	wait1Msec(5000);
	powerOff();
}

//Written by: Melvin Wang
task eStop()
{
	while(message!=EMERGENCY_SHUTDOWN){}
	displayCenteredTextLine(0, "EMERGENCY SHUT DOWN!");
	powerOff();
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
	motor[yMotor] = -XY_CAL_POWER_FAST;
	while(SensorValue[yTouch] == 0){}
	nMotorEncoder[yMotor] = 0;
	//move back one rotation
	motor[yMotor] = XY_CAL_POWER_FAST;
	while(fabs(nMotorEncoder[yMotor] * Y_GEAR_RATIO * Y_GEAR_CIRCUMFERENCE / ENCODER_PPR) < 1){}
	//calibrate slowly
	motor[yMotor] = -XY_CAL_POWER_SLOW;
	while(SensorValue[yTouch] == 0){}
	motor[yMotor] = 0;
	nMotorEncoder[yMotor] = 0;

	motor[xMotor] = -XY_CAL_POWER_FAST;
	while(SensorValue[xTouch] == 0){}
	nMotorEncoder[xMotor] = 0;
	//move back one rotation
	motor[xMotor] = XY_CAL_POWER_FAST;
	while(fabs(nMotorEncoder[xMotor] * X_GEAR_RATIO * X_GEAR_CIRCUMFERENCE / ENCODER_PPR) < 1 ){}
	//calibrate slowly
	motor[xMotor] = -XY_CAL_POWER_SLOW;
	while(SensorValue[xTouch] == 0){}
	motor[xMotor] = 0;
	nMotorEncoder[xMotor] = 0;
	sendMessage(COMPLETE);
}

//Written by: Melvin Wang
void receiveFile()
{
	displayCenteredTextLine(0, "Receiving File");
	TFileHandle fileHandle;
	TFileIOResult fileIOResult;
	string fileName = "receive.txt";///
	unsigned short fileSize;

	Delete(fileName, fileIOResult);

	while(message==NO_MESSAGE){}
	fileSize = message;
	ClearMessage();
	displayCenteredTextLine(1, "%d bytes", fileSize);
	OpenWrite(fileHandle, fileIOResult, fileName, fileSize);

	for (int fileIndex = 0; fileIndex< fileSize/6; fileIndex++)
	{
		for (int index = 0; index<3; index++)
		{
			while(message==NO_MESSAGE){}
			WriteShort(fileHandle, fileIOResult, messageParm[index]);
		}
		ClearMessage();
	}
	//in case file is not multiple of three shorts (6bytes)
	if(fileSize%6!=0)
	{
		byte character = 0;
		for(int remainder = 0; remainder < fileSize%6; remainder++)
		{
			while(message==NO_MESSAGE){}
			character = message;
			ClearMessage();
			WriteByte(fileHandle, fileIOResult, character);
		}
	}
	eraseDisplay();
	Close(fileHandle, fileIOResult);
}

//Written by: Gabriel Mok
void executeJob()
{
	displayCenteredTextLine(0, "Executing Job");
	TFileHandle fileHandle;
	TFileIOResult fileIOResult;
	string fileName = "";
	unsigned short fileSize = 0;

	FindFirstFile(fileHandle, fileIOResult, "receive.txt", fileName, fileSize);

	OpenRead(fileHandle, fileIOResult, fileName, fileSize);

	float xPower = 0;
	float yPower = 0;
	int xDistance = 0;
	int yDistance = 0;
	getNextPoint(fileHandle, fileIOResult, xPower, yPower, xDistance, yDistance);
	bool firstPoint = true;

	while(xDistance != END_OF_JOB)
	{
		//if (xPower == 0 && yPower == 0 && xDistance == 1)
		//{
		//	displayCenteredTextLine(1, "Lifiting Spindle");
		//	sendMessage(SPINDLE_UP);
		//	while(message != COMPLETE){}
		//	ClearMessage();
		//	getNextPoint(fileHandle, fileIOResult, xPower, yPower, xDistance, yDistance);

		//	if(xDistance != -2)
		//	{
		//		moveXYMotors(xPower, yPower, xDistance, yDistance);
		//		displayCenteredTextLine(1, "Lowering Spindle");
		//		sendMessage(SPINDLE_DOWN);
		//		while(message != COMPLETE){}
		//		ClearMessage();
		//		displayCenteredTextLine(1, "");
		//		getNextPoint(fileHandle, fileIOResult, xPower, yPower, xDistance, yDistance);
		//	}
		//}
		//else
		{
			moveXYMotors(xPower, yPower, xDistance, yDistance);
			if(firstPoint)
			{
				displayCenteredTextLine(1, "Lowering Spindle");
				sendMessage(SPINDLE_DOWN);
				while(message != COMPLETE){}
				ClearMessage();
				displayCenteredTextLine(1, "");
				firstPoint = false;
			}
		getNextPoint(fileHandle, fileIOResult, xPower, yPower, xDistance, yDistance);		}
	}
	eraseDisplay();
	displayCenteredTextLine(0, "End of Job");
	sendMessage(COMPLETE);
	wait1Msec(30);
	Close(fileHandle, fileIOResult);
	Delete(fileName, fileIOResult);

}

//Written by: Gabriel Mok
void getNextPoint(TFileHandle &fileHandle, TFileIOResult &fileIOResult, float &xPower, float &yPower, int &xDistance, int &yDistance)
{
	char nextChar = '\0';
	string inputStr = "";
	ReadByte(fileHandle, fileIOResult, nextChar);
	while(!validChar(nextChar))
	{
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	while(validChar(nextChar))
	{
		inputStr += nextChar;
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	xPower = atof(inputStr);

	inputStr = "";
	ReadByte(fileHandle, fileIOResult, nextChar);
	while(!validChar(nextChar))
	{
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	while(validChar(nextChar))
	{
		inputStr += nextChar;
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	yPower = atof(inputStr);

	inputStr = "";
	ReadByte(fileHandle, fileIOResult, nextChar);
	while(!validChar(nextChar))
	{
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	while(validChar(nextChar))
	{
		inputStr += nextChar;
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	xDistance = atoi(inputStr);

	inputStr = "";
	ReadByte(fileHandle, fileIOResult, nextChar);
	while(!validChar(nextChar))
	{
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	while(validChar(nextChar))
	{
		inputStr += nextChar;
		ReadByte(fileHandle, fileIOResult, nextChar);
	}
	yDistance = atoi(inputStr);
}

//Written by: Melvin Wang
bool validChar(char testChar)
{
	if (testChar == 45 || testChar == 46 || (testChar >= 48 && testChar <= 57))
	{
		return true;
	}
	return false;
}

//Written by: Gabriel Mok
void moveXYMotors(float xPower, float yPower, int xDistance, int yDistance)
{


	float xPrevPos = nMotorEncoder[xMotor];
	float yPrevPos = nMotorEncoder[yMotor];

	motor[xMotor] = -xPower * SPEED_CONSTANT;
	motor[yMotor] = yPower * SPEED_CONSTANT;
	while (fabs(nMotorEncoder[xMotor] - xPrevPos)*X_GEAR_RATIO*X_GEAR_CIRCUMFERENCE/ENCODER_PPR < xDistance/PIXELS_PER_INCH || fabs(nMotorEncoder[yMotor] - yPrevPos) *Y_GEAR_RATIO*Y_GEAR_CIRCUMFERENCE/ENCODER_PPR< yDistance/PIXELS_PER_INCH)////////////PPPPRRRRRR UNITS ALSO ASSUME SIGN OF DIST IS ALWAYS POSitivie
	{
		displayCenteredTextLine(1, "x encoder: %d",nMotorEncoder[xMotor]);
		displayCenteredTextLine(2, "y encoder: %d",nMotorEncoder[yMotor]);

		if (fabs(nMotorEncoder[xMotor] - xPrevPos) *X_GEAR_RATIO*X_GEAR_CIRCUMFERENCE/ENCODER_PPR>= xDistance/PIXELS_PER_INCH)
		{
			motor[xMotor] = 0;
		}
		if (fabs(nMotorEncoder[yMotor] - yPrevPos)*Y_GEAR_RATIO*Y_GEAR_CIRCUMFERENCE/ENCODER_PPR >= yDistance/PIXELS_PER_INCH)
		{
			motor[yMotor] = 0;
		}
	}
	motor[xMotor] = motor[yMotor] = 0;
}
