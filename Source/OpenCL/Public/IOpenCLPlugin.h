// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "OCLData.h"


/**
 * The public interface to this module.  In most cases, this interface is only public to sibling modules
 * within this plugin.
 */
class IOpenCLPlugin : public IModuleInterface
{

public:

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	static inline IOpenCLPlugin& Get()
	{
		return FModuleManager::LoadModuleChecked<IOpenCLPlugin>("OpenCL");
	}

	/**
	 * Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	 *
	 * @return True if the module is loaded and ready to use
	 */
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("OpenCL");
	}

	virtual void EnumerateDevices(TArray<FOpenCLDeviceData>& OutDevices, bool bForceRefresh = false) {};
	virtual void RunKernelOnDevices(const FString& KernelString,
									const FString& KernelName,
									const FString& Args,
									TFunction<void(const FString&, bool)> ResultCallback,
									const TArray<FOpenCLDeviceData>& OutDevices) {};

};

DECLARE_LOG_CATEGORY_EXTERN(LogOpenCL, Log, All);

