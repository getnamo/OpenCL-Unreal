#include "OpenCLComponent.h"
#include "EngineMinimal.h"
#include "Async/Async.h"
#include "IOpenCLPlugin.h"

UOpenCLComponent::UOpenCLComponent(const FObjectInitializer &init) : UActorComponent(init)
{
	bWantsInitializeComponent = true;
	bAutoActivate = true;
	bWatchKernelsFolderOnStartup = true;
	WatchedDiskReadDelay = 0.05f;
	WatchedNotificationLockout = 0.5f;

	DeviceGroup = EnumerateDevices();
}

bool UOpenCLComponent::IsWatchingFolders()
{
	return WatchedFolders.Num() > 0;
}

bool UOpenCLComponent::HasValidHardware()
{
	TArray<FOpenCLDeviceData> CLDevices = EnumerateDevices();
	IOpenCLPlugin::Get().EnumerateDevices(CLDevices);
	return CLDevices.Num() > 0;
}

TArray<FOpenCLDeviceData> UOpenCLComponent::EnumerateDevices()
{
	TArray<FOpenCLDeviceData> CLDevices;
	if (IOpenCLPlugin::Get().IsAvailable())
	{
		IOpenCLPlugin::Get().EnumerateDevices(CLDevices);
	}
	return CLDevices;
}

bool UOpenCLComponent::ReadKernelFromFile(const FString& FilePath, FString& OutKernelSource, bool bIsContentRelative /*= true*/)
{
	FString AbsolutePath;
	if (bIsContentRelative)
	{
		AbsolutePath = FPaths::ProjectContentDir() + FilePath;
	}
	else
	{
		AbsolutePath = FilePath;
	}

	return FFileHelper::LoadFileToString(OutKernelSource, *AbsolutePath);
}

void UOpenCLComponent::RunOpenCLKernel(const FString& Kernel, const FString& KernelName /*= TEXT("main")*/, const FString& InputArgs /*= TEXT("")*/)
{
	if (IOpenCLPlugin::Get().IsAvailable())
	{
		IOpenCLPlugin::Get().RunKernelOnDevices(Kernel, KernelName, InputArgs, [this](const FString& Result, bool Success)
		{
			OnResult.Broadcast(Result, Success);
		}, DeviceGroup);
	}
}

void UOpenCLComponent::WatchKernelFolder(const FString& ProjectRelativeFolder)
{
	UnwatchKernelFolder(ProjectRelativeFolder);
	FString AbsolutePath = FPaths::ProjectContentDir() + ProjectRelativeFolder;

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

	Changed = IDirectoryWatcher::FDirectoryChanged::CreateLambda([&](const TArray<FFileChangeData>& FileChanges) {

		FTimespan Difference = FDateTime::Now() - LastWatchEventCall;

		//Rate limit file change callbacks
		if (Difference.GetTotalSeconds() > WatchedNotificationLockout)
		{
			FTimerHandle UniqueHandle;
			FTimerDelegate TimerCallback;
			TimerCallback.BindLambda([this, FileChanges]
			{
				for (auto Change : FileChanges)
				{
					FPaths::NormalizeFilename(Change.Filename);
					OnKernelSourceChanged.Broadcast(Change.Filename, (EKernelFileChangeAction)Change.Action);
				}
			});
			GetWorld()->GetTimerManager().SetTimer(UniqueHandle, TimerCallback, WatchedDiskReadDelay, false);
			LastWatchEventCall = FDateTime::Now();
		}
	});

	if (IFileManager::Get().DirectoryExists(*AbsolutePath))
	{
		DirectoryWatcher->RegisterDirectoryChangedCallback_Handle(AbsolutePath, Changed, DelegateHandle, true);
		WatchedFolders.Add(ProjectRelativeFolder);
	}
	else
	{
		UE_LOG(LogOpenCL, Log, TEXT("WatchKernelFolder:: %s folder doesn't exist."), *AbsolutePath);
	}
}

void UOpenCLComponent::UnwatchKernelFolder(const FString& ProjectRelativeFolder)
{
	FString AbsolutePath = FPaths::ProjectContentDir() + ProjectRelativeFolder;

	FDirectoryWatcherModule& DirectoryWatcherModule = FModuleManager::Get().LoadModuleChecked<FDirectoryWatcherModule>(TEXT("DirectoryWatcher"));
	IDirectoryWatcher* DirectoryWatcher = DirectoryWatcherModule.Get();

	if (IFileManager::Get().DirectoryExists(*AbsolutePath))
	{
		DirectoryWatcher->UnregisterDirectoryChangedCallback_Handle(AbsolutePath, DelegateHandle);
		WatchedFolders.Remove(ProjectRelativeFolder);
	}
	else
	{
		UE_LOG(LogOpenCL, Log, TEXT("UnwatchKernelFolder:: %s folder doesn't exist."), *AbsolutePath);
	}
}

void UOpenCLComponent::InitializeComponent()
{
	Super::InitializeComponent();
	DeviceGroup = EnumerateDevices();

	LastWatchEventCall = FDateTime::Now();
	UWorld* World = GEngine->GetWorldFromContextObject(this, EGetWorldErrorMode::LogAndReturnNull);
	if (bWatchKernelsFolderOnStartup && World->IsGameWorld())
	{
		WatchKernelFolder();
	}
	
}

void UOpenCLComponent::UninitializeComponent()
{	
	TArray<FString> AllWatched = WatchedFolders;
	for (auto Folder : AllWatched)
	{
		UnwatchKernelFolder(Folder);
	}
	Super::UninitializeComponent();
}
