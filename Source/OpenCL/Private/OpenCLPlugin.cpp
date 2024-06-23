// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"
#include "IOpenCLPlugin.h"
#include "CL/opencl.h"
#include "OCLUtility.h"
#include <string>	//this needs to be included to change how char* works

class OpenCLPlugin : public IOpenCLPlugin
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	virtual void EnumerateDevices(TArray<FOpenCLDeviceData>& OutDevices, bool bForceRefresh = false) override;
	virtual void RunKernelOnDevices(const FString& KernelString, const FString& KernelName, const FString& Args, TFunction<void(const FString&, bool)> ResultCallback, const TArray<FOpenCLDeviceData>& OutDevices) override;

private:
	TArray<FOpenCLDeviceData> CLDevices;

	TArray<cl_device_id*> DeviceIdsMemoryList;
	bool bHasEnumeratedOnce;

	void FreeDeviceMemory();
};

IMPLEMENT_MODULE( OpenCLPlugin, OpenCL )
DEFINE_LOG_CATEGORY(LogOpenCL);

void OpenCLPlugin::StartupModule()
{
	//Enumerate once on startup
	bHasEnumeratedOnce = false;
	EnumerateDevices(CLDevices);

	//Log all devices
	UE_LOG(LogOpenCL, Log, TEXT("OpenCL Info:"));
	for (auto CLDevice : CLDevices)
	{
		UE_LOG(LogOpenCL, Log, TEXT("%s"), *CLDevice.ToPrintString());
	}
}

void OpenCLPlugin::ShutdownModule() 
{
	FreeDeviceMemory();
}


void OpenCLPlugin::EnumerateDevices(TArray<FOpenCLDeviceData>& OutDevices, bool bForceRefresh /*= false*/)
{
	if (bHasEnumeratedOnce && !bForceRefresh)
	{
		OutDevices = CLDevices;
		return;
	}

	FreeDeviceMemory();

	cl_uint i, j;
	char* Value;
	size_t ValueSize;
	cl_uint NumPlatforms;
	cl_platform_id* Platforms;
	cl_uint NumDevices;
	cl_device_id* Devices;
	cl_uint MaxComputeUnits;

	clGetPlatformIDs(0, NULL, &NumPlatforms);
	Platforms = (cl_platform_id*)malloc(sizeof(cl_platform_id) * NumPlatforms);
	clGetPlatformIDs(NumPlatforms, Platforms, NULL);

	//For each platform
	for (i = 0; i < NumPlatforms; i++) {

		clGetPlatformInfo(Platforms[i], CL_PLATFORM_NAME, 0, NULL, &ValueSize);
		Value = (char*)malloc(ValueSize);
		clGetPlatformInfo(Platforms[i], CL_PLATFORM_NAME, ValueSize, Value, NULL);
		FString EnumeratedPlatform = FString(ANSI_TO_TCHAR(Value));
		free(Value);

		// get all Devices
		clGetDeviceIDs(Platforms[i], CL_DEVICE_TYPE_ALL, 0, NULL, &NumDevices);
		Devices = (cl_device_id*)malloc(sizeof(cl_device_id) * NumDevices);
		clGetDeviceIDs(Platforms[i], CL_DEVICE_TYPE_ALL, NumDevices, Devices, NULL);

		DeviceIdsMemoryList.Add(Devices);

		//For each device
		for (j = 0; j < NumDevices; j++) {
			FOpenCLDeviceData Device;
			Device.Platform = EnumeratedPlatform;
			Device.DeviceId = FString::Printf(TEXT("%d"), ::PointerHash(Devices[j]));
			Device.RawDeviceId = Devices[j];

			clGetDeviceInfo(Devices[j], CL_DEVICE_NAME, 0, NULL, &ValueSize);
			Value = (char*)malloc(ValueSize);
			clGetDeviceInfo(Devices[j], CL_DEVICE_NAME, ValueSize, Value, NULL);
			Device.DeviceName = ANSI_TO_TCHAR(Value);
			free(Value);

			clGetDeviceInfo(Devices[j], CL_DEVICE_VERSION, 0, NULL, &ValueSize);
			Value = (char*)malloc(ValueSize);
			clGetDeviceInfo(Devices[j], CL_DEVICE_VERSION, ValueSize, Value, NULL);
			Device.HardwareVersion = ANSI_TO_TCHAR(Value);
			free(Value);

			clGetDeviceInfo(Devices[j], CL_DRIVER_VERSION, 0, NULL, &ValueSize);
			Value = (char*)malloc(ValueSize);
			clGetDeviceInfo(Devices[j], CL_DRIVER_VERSION, ValueSize, Value, NULL);
			Device.SoftwareVersion = ANSI_TO_TCHAR(Value);
			free(Value);

			clGetDeviceInfo(Devices[j], CL_DEVICE_OPENCL_C_VERSION, 0, NULL, &ValueSize);
			Value = (char*)malloc(ValueSize);
			clGetDeviceInfo(Devices[j], CL_DEVICE_OPENCL_C_VERSION, ValueSize, Value, NULL);
			Device.OpenCLVersion = ANSI_TO_TCHAR(Value);
			free(Value);

			clGetDeviceInfo(Devices[j], CL_DEVICE_MAX_COMPUTE_UNITS, sizeof(MaxComputeUnits), &MaxComputeUnits, NULL);
			Device.ParallelComputeUnits = MaxComputeUnits;
			OutDevices.Add(Device);
		}
		//free(Devices);
	}
	free(Platforms);

	bHasEnumeratedOnce = true;
}

void OpenCLPlugin::RunKernelOnDevices(const FString& KernelString, const FString& KernelName, const FString& Args, TFunction<void(const FString&, bool)> ResultCallback, const TArray<FOpenCLDeviceData>& OutDevices)
{
	//For now we only run this on the top level device in the group

	if (OutDevices.Num() == 0)
	{
		UE_LOG(LogOpenCL, Log, TEXT("RunKernelOnDevices:: No devices found in devicegroup."));
		return;
	}
	//grab top level device for now, todo: run for entire group?
	const FOpenCLDeviceData& Device = OutDevices[0];

	cl_device_id DeviceId = (cl_device_id)Device.RawDeviceId;

	if (DeviceId == nullptr)
	{
		UE_LOG(LogOpenCL, Log, TEXT("Invalid device id"));
		return;	//todo: continue/skip
	}

	/* Create OpenCL context */
	cl_int Ret;
	cl_context Context = nullptr;
	cl_command_queue CommandQueue = nullptr;
	cl_mem MemObj = nullptr;
	cl_program Program = nullptr;
	cl_build_status BuildStatus;
	cl_kernel Kernel = nullptr;
	TArray<uint8> SourceCharArray;
	const int32 MemSize = 128;

	Context = clCreateContext(NULL, 1, &DeviceId, NULL, NULL, &Ret);

	/* Create Command Queue */
	CommandQueue = clCreateCommandQueue(Context, DeviceId, 0, &Ret);

	/* Create Memory Buffer */
	MemObj = clCreateBuffer(Context, CL_MEM_READ_WRITE, MemSize * sizeof(char), NULL, &Ret);

	/* Create Kernel Program from the source */

	//We need a separately allocated char array.
	SourceCharArray = FOCLUtility::FStringToCharArray(KernelString);

	const size_t SourceLen = KernelString.Len();
	const char* SourceStr = (char*)SourceCharArray.GetData();
	
	Program = clCreateProgramWithSource(Context, 1, &SourceStr, &SourceLen, &Ret);

	/* Build Kernel Program */
	Ret = clBuildProgram(Program, 1, &DeviceId, NULL, NULL, NULL);

	clGetProgramBuildInfo(Program, DeviceId, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status), &BuildStatus, NULL);


	if (BuildStatus != CL_SUCCESS)
	{
		char *BuildLog;
		size_t RetValSize;
		FString ErrorMessage = TEXT("RunKernelOnDevices::CL_BUILD_PROGRAM_FAILURE\n");

		clGetProgramBuildInfo(Program, DeviceId, CL_PROGRAM_BUILD_LOG, 0, NULL, &RetValSize);

		BuildLog = new char[RetValSize + 1];

		clGetProgramBuildInfo(Program, DeviceId, CL_PROGRAM_BUILD_LOG, RetValSize, BuildLog, NULL);
		BuildLog[RetValSize] = '\0';

		ErrorMessage += FString(BuildLog);

		UE_LOG(LogOpenCL, Warning, TEXT("%s"), *ErrorMessage);
		ResultCallback(ErrorMessage, false);

		delete[] BuildLog;
		Ret = clReleaseProgram(Program);
		Ret = clReleaseCommandQueue(CommandQueue);
		return;
	}

	/* Create OpenCL Kernel */
	Kernel = clCreateKernel(Program, TCHAR_TO_ANSI(*KernelName), &Ret);

	if (Ret == CL_INVALID_PROGRAM_EXECUTABLE)
	{
		UE_LOG(LogOpenCL, Warning, TEXT("RunKernelOnDevices:: Error:CL_INVALID_PROGRAM_EXECUTABLE"));
		ResultCallback(TEXT("Error: CL_INVALID_PROGRAM_EXECUTABLE."), false);
		Ret = clReleaseKernel(Kernel);
		Ret = clReleaseProgram(Program);
		Ret = clReleaseCommandQueue(CommandQueue);
		return;
	}

	/* Set OpenCL Kernel Parameters */
	Ret = clSetKernelArg(Kernel, 0, sizeof(cl_mem), (void *)&MemObj);

	/* Execute OpenCL Kernel */
	Ret = clEnqueueTask(CommandQueue, Kernel, 0, NULL, NULL);

	/* Copy results from the memory buffer */
	char ReturnString[MemSize];

	Ret = clEnqueueReadBuffer(CommandQueue, MemObj, CL_TRUE, 0, MemSize * sizeof(char), ReturnString, 0, NULL, NULL);

	/* Display Result */
	ResultCallback(FString(ANSI_TO_TCHAR(ReturnString)), true);

	Ret = clFlush(CommandQueue);
	Ret = clFinish(CommandQueue);
	Ret = clReleaseKernel(Kernel);
	Ret = clReleaseProgram(Program);
	Ret = clReleaseMemObject(MemObj);
	Ret = clReleaseCommandQueue(CommandQueue);
	Ret = clReleaseContext(Context);
}

void OpenCLPlugin::FreeDeviceMemory()
{
	CLDevices.Empty();

	//Free each of our list of lists
	for (auto DeviceList : DeviceIdsMemoryList)
	{
		free(DeviceList);
	}
	DeviceIdsMemoryList.Empty();
}
