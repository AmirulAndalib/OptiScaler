#include "DLSSFeature_Dx11.h"
#include <dxgi.h>
#include "../../Config.h"
#include "../../pch.h"

bool DLSSFeatureDx11::Init(ID3D11Device* InDevice, ID3D11DeviceContext* InContext, NVSDK_NGX_Parameter* InParameters)
{
	if (NVNGXProxy::NVNGXModule() == nullptr)
	{
		LOG_ERROR("nvngx.dll not loaded!");

		SetInit(false);
		return false;
	}

	NVSDK_NGX_Result nvResult;
	bool initResult = false;

	Device = InDevice;
	DeviceContext = InContext;

	do
	{
		if (!_dlssInited)
		{
			_dlssInited = NVNGXProxy::InitDx11(InDevice);

			if (!_dlssInited)
				return false;

			_moduleLoaded = (NVNGXProxy::D3D11_Init_ProjectID() != nullptr || NVNGXProxy::D3D11_Init_Ext() != nullptr) &&
				(NVNGXProxy::D3D11_Shutdown() != nullptr || NVNGXProxy::D3D11_Shutdown1() != nullptr) &&
				(NVNGXProxy::D3D11_GetParameters() != nullptr || NVNGXProxy::D3D11_AllocateParameters() != nullptr) && NVNGXProxy::D3D11_DestroyParameters() != nullptr &&
				NVNGXProxy::D3D11_CreateFeature() != nullptr && NVNGXProxy::D3D11_ReleaseFeature() != nullptr && NVNGXProxy::D3D11_EvaluateFeature() != nullptr;

			//delay between init and create feature
			std::this_thread::sleep_for(std::chrono::milliseconds(500));
		}

		LOG_INFO("Creating DLSS feature");

		if (NVNGXProxy::D3D11_CreateFeature() != nullptr)
		{
			ProcessInitParams(InParameters);

			_p_dlssHandle = &_dlssHandle;
			nvResult = NVNGXProxy::D3D11_CreateFeature()(InContext, NVSDK_NGX_Feature_SuperSampling, InParameters, &_p_dlssHandle);

			if (nvResult != NVSDK_NGX_Result_Success)
			{
				LOG_ERROR("_CreateFeature result: {0:X}", (unsigned int)nvResult);
				break;
			}
		}
		else
		{
			LOG_ERROR("_CreateFeature is nullptr");
			break;
		}

		ReadVersion();

		initResult = true;

	} while (false);

	if (initResult)
	{
		if (!Config::Instance()->OverlayMenu.value_or(true) && (Imgui == nullptr || Imgui.get() == nullptr))
			Imgui = std::make_unique<Imgui_Dx11>(GetForegroundWindow(), InDevice);
	}

	SetInit(initResult);

	return initResult;
}

bool DLSSFeatureDx11::Evaluate(ID3D11DeviceContext* InDeviceContext, NVSDK_NGX_Parameter* InParameters)
{
	if (!_moduleLoaded)
	{
		LOG_ERROR("nvngx.dll or _nvngx.dll is not loaded!");
		return false;
	}

	NVSDK_NGX_Result nvResult;
	ID3D11Resource* paramOutput;

	if (NVNGXProxy::D3D11_EvaluateFeature() != nullptr)
	{
		ProcessEvaluateParams(InParameters);

		nvResult = NVNGXProxy::D3D11_EvaluateFeature()(InDeviceContext, _p_dlssHandle, InParameters, NULL);

		if (nvResult != NVSDK_NGX_Result_Success)
		{
			LOG_ERROR("_EvaluateFeature result: {0:X}", (unsigned int)nvResult);
			return false;
		}
		else
		{
			LOG_TRACE("_EvaluateFeature ok!");
		}
	}
	else
	{
		LOG_ERROR("_EvaluateFeature is nullptr");
		return false;
	}

	// imgui
	if (!Config::Instance()->OverlayMenu.value_or(true) && _frameCount > 30 && 
		InParameters->Get(NVSDK_NGX_Parameter_Output, &paramOutput) == NVSDK_NGX_Result_Success)
	{
		if (Imgui != nullptr && Imgui.get() != nullptr)
		{
			if (Imgui->IsHandleDifferent())
				Imgui.reset();
			else
				Imgui->Render(InDeviceContext, paramOutput);
		}
		else
		{
			if (Imgui == nullptr || Imgui.get() == nullptr)
				Imgui = std::make_unique<Imgui_Dx11>(GetForegroundWindow(), Device);
		}
	}

	_frameCount++;

	return true;
}

void DLSSFeatureDx11::Shutdown(ID3D11Device* InDevice)
{
	if (_dlssInited)
	{
		if (NVNGXProxy::D3D11_Shutdown() != nullptr)
			NVNGXProxy::D3D11_Shutdown()();
		else if (NVNGXProxy::D3D11_Shutdown1() != nullptr)
			NVNGXProxy::D3D11_Shutdown1()(InDevice);
	}

	DLSSFeature::Shutdown();
}

DLSSFeatureDx11::DLSSFeatureDx11(unsigned int InHandleId, NVSDK_NGX_Parameter* InParameters) : IFeature(InHandleId, InParameters), IFeature_Dx11(InHandleId, InParameters), DLSSFeature(InHandleId, InParameters)
{
	if (NVNGXProxy::NVNGXModule() == nullptr)
	{
		LOG_INFO("nvngx.dll not loaded, now loading");
		NVNGXProxy::InitNVNGX();
	}

	LOG_INFO("binding complete!");
}

DLSSFeatureDx11::~DLSSFeatureDx11()
{
	if (NVNGXProxy::D3D11_ReleaseFeature() != nullptr && _p_dlssHandle != nullptr)
		NVNGXProxy::D3D11_ReleaseFeature()(_p_dlssHandle);
}
