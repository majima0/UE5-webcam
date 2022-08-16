// Fill out your copyright notice in the Description page of Project Settings.


#include "MyUserWidget.h"
#include <fstream>
#include <iostream>
#include <array>
#include <string>
#include <filesystem>


#include "PreOpenCVHeaders.h"
#include "OpenCVHelper.h"
#include <ThirdParty/OpenCV/include/opencv2/imgproc.hpp>
#include <ThirdParty/OpenCV/include/opencv2/highgui/highgui.hpp>
#include <ThirdParty/OpenCV/include/opencv2/core.hpp>
#include "PostOpenCVHeaders.h"
//#include "MediaBundle.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Texture2D.h"
using namespace std;
using namespace cv;

void UMyUserWidget::NativeConstruct()
{
	Super::NativeConstruct();
	// Initialize OpenCV and webcam properties
	CameraID = 0;
	RefreshRate = 15;
	IsStreamOpen = false;
	VideoSize = FVector2D(0, 0);
	ShouldResize = false;
	ResizeDimensions = FVector2D(320, 240);
	RefreshTimer = 0.0f;
	CVVideo = cv::VideoCapture();
	CVFrame = cv::Mat();

	CVVideo.open(CameraID);
	if (CVVideo.isOpened()) {
		// Initialize stream
		IsStreamOpen = true;
		UpdateFrame();
		VideoSize = FVector2D(CVFrame.cols, CVFrame.rows);
		CVSize = cv::Size(ResizeDimensions.X, ResizeDimensions.Y);
		VideoTexture = UTexture2D::CreateTransient(VideoSize.X, VideoSize.Y);
		VideoTexture->UpdateResource();
		VideoUpdateTextureRegion =
			new FUpdateTextureRegion2D(
				0,
				0,
				0,
				0,
				VideoSize.X,
				VideoSize.Y);

		// Initialize data array
		Data.Init(FColor(0, 0, 0255), VideoSize.X * VideoSize.Y);

		// Do first frame
		UpdateTexture();
		OnNextVideoFrame();
	}
}

void UMyUserWidget::NativeTick(const FGeometry& g, float InDeltaTime)
{
	Super::NativeTick(g, InDeltaTime);

	RefreshTimer += InDeltaTime;
	if (IsStreamOpen && RefreshTimer >= 1.0f / RefreshRate) {
		RefreshTimer -= 1.0f / RefreshRate;

		UpdateFrame();
		UpdateTexture();
		OnNextVideoFrame();
	}
}

void UMyUserWidget::UpdateFrame() {
	if (CVVideo.isOpened()) {
		CVVideo.read(CVFrame);
		if (ShouldResize) {
			cv::resize(CVFrame, CVFrame, CVSize);
		}
	}
	else {
		IsStreamOpen = false;
	}
}

void UMyUserWidget::UpdateTexture() {
	if (IsStreamOpen && CVFrame.data) {
		for (int y = 0; y < VideoSize.Y; y++) {
			for (int x = 0; x < VideoSize.X; x++) {
				int i = x + (y * VideoSize.X);
				Data[i].B = CVFrame.data[i * 3 + 0];
				Data[i].G = CVFrame.data[i * 3 + 1];
				Data[i].R = CVFrame.data[i * 3 + 2];
			}
		}

		UpdateTextureRegions(
			VideoTexture,
			(int32)0,
			(uint32)1,
			VideoUpdateTextureRegion,
			(uint32)(4 * VideoSize.X),
			(uint32)4,
			(uint8*)Data.GetData(),
			false);
	}
}

void UMyUserWidget::UpdateTextureRegions(
	UTexture2D* Texture,
	int32 MipIndex,
	uint32 NumRegions,
	FUpdateTextureRegion2D* Regions,
	uint32 SrcPitch,
	uint32 SrcBpp,
	uint8* SrcData,
	bool bFreeData) {
	if (Texture->Resource) {
		struct FUpdateTextureRegionsData {
			FTextureResource* TextueResource;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->TextueResource = (FTextureResource*)Texture->Resource;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_RENDER_COMMAND(FUpdateTextureRegionsData)(
			[RegionData, bFreeData](FRHICommandListImmediate& RHICmdList) {
				for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex) {
					RHIUpdateTexture2D(
						RegionData->TextueResource->TextureRHI->GetTexture2D(),
						0,
						RegionData->Regions[RegionIndex],
						RegionData->SrcPitch,
						RegionData->SrcData
						+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
						+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
					);
				}
				if (bFreeData) {
					FMemory::Free(RegionData->Regions);
					FMemory::Free(RegionData->SrcData);
				}
				delete RegionData;
			}
		);
	}
}

UTexture2D* UMyUserWidget::GetValue2() {
	return VideoTexture;
}