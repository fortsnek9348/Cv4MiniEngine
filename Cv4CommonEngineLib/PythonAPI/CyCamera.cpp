#include "CyCamera.h"
#include "CyCommon.h"

#include <CyPlot.h>
#include <CyUnit.h>

void CyCamera::registerWithPython(const pybind11::module& m)
{
#define R(x) def(#x, &CyCamera::x)

	pybind11::class_<CyCamera>(m, "CyCamera")
		.def(pybind11::init())
		.R(GetBasePitch)
		.R(GetBaseTurn)
		.R(GetCameraMovementSpeed)
		.R(GetCurrentPosition)
		.R(GetDefaultViewPortCenter)
		.R(GetDestinationPosition)
		//.R(GetLookAt)
		//.R(GetLookAtSpeed)
		.R(GetTargetDestination)
		.R(GetZoom)
		.R(JustLookAt)
		.R(JustLookAtPlot)
		.R(LookAt)
		.R(LookAtUnit)
		.R(MoveBaseTurnLeft)
		.R(MoveBaseTurnRight)
		.R(ReleaseLockedCamera)
		.R(ResetZoom)
		.R(SetBasePitch)
		.R(SetBaseTurn)
		.R(SetCameraMovementSpeed)
		.R(SetCurrentPosition)
		.R(SetDestinationPosition)
		.R(SetLookAtSpeed)
		.R(SetTargetDestination)
		.R(SetViewPortCenter)
		.R(SetZoom)
		.R(SimpleLookAt)
		.R(Translate)
		.R(ZoomIn)
		.R(ZoomOut)
		.R(isMoving)
		.R(setOrthoCamera)
		;
}

namespace
{
	// TODO: Get rid of this? Use actual WorldView camera?
	struct VirtualCameraState
	{
		float pitch = 0;
		float turn = 0;
		float zoom = 0;
		NiPoint3 position{ 0, 0, 0 };

		float speed = 0;
		float lookAtSpeed = 0;

		NiPoint3 target{ 0, 0, 0 };

		NiPoint2 viewportCenter{ 0, 0 };

		bool ortho = false;
	};

	constinit VirtualCameraState gVirtualCameraState{};
}

float CyCamera::GetBasePitch()
{
	return gVirtualCameraState.pitch;
}

float CyCamera::GetBaseTurn()
{
	return gVirtualCameraState.turn;
}

float CyCamera::GetCameraMovementSpeed()
{
	return gVirtualCameraState.speed;
}

NiPoint3 CyCamera::GetCurrentPosition()
{
	return gVirtualCameraState.position;
}

NiPoint2 CyCamera::GetDefaultViewPortCenter()
{
	return VirtualCameraState().viewportCenter;
}

NiPoint3 CyCamera::GetDestinationPosition()
{
	return gVirtualCameraState.position;
}

//void CyCamera::GetLookAt(NiPoint3 pt3LookAt)
//{
//	unimplementedPythonFunction();
//}
//
//float CyCamera::GetLookAtSpeed()
//{
//	unimplementedPythonFunction();
//}

NiPoint3 CyCamera::GetTargetDestination()
{
	return gVirtualCameraState.target;
}

float CyCamera::GetZoom()
{
	return gVirtualCameraState.zoom;
}

void CyCamera::JustLookAt(NiPoint3 p3LookAt)
{
	gVirtualCameraState.target = p3LookAt;
}

void CyCamera::JustLookAtPlot([[maybe_unused]] CyPlot pPlot)
{
	cvengine::abortOnUnimplementedPythonFunction();
}

void CyCamera::LookAt(NiPoint3 pt3LookAt, [[maybe_unused]] CameraLookAtTypes type, [[maybe_unused]] NiPoint3 attackDirection)
{
	gVirtualCameraState.target = pt3LookAt;
}

void CyCamera::LookAtUnit([[maybe_unused]] CyUnit unit)
{
	cvengine::abortOnUnimplementedPythonFunction();
}

void CyCamera::MoveBaseTurnLeft(float increment)
{
	// ??
	gVirtualCameraState.turn += increment;
}

void CyCamera::MoveBaseTurnRight(float increment)
{
	// ??
	gVirtualCameraState.turn -= increment;
}

void CyCamera::ReleaseLockedCamera()
{
	cvengine::abortOnUnimplementedPythonFunction();
}

void CyCamera::ResetZoom()
{
	gVirtualCameraState.zoom = VirtualCameraState().zoom;
}

void CyCamera::SetBasePitch(float fBasePitch)
{
	gVirtualCameraState.pitch = fBasePitch;
}

void CyCamera::SetBaseTurn(float baseTurn)
{
	gVirtualCameraState.turn = baseTurn;
}

void CyCamera::SetCameraMovementSpeed(CameraMovementSpeeds eSpeed)
{
	// ??
	gVirtualCameraState.speed = float(eSpeed);
}

void CyCamera::SetCurrentPosition(NiPoint3 point)
{
	gVirtualCameraState.position = point;
}

void CyCamera::SetDestinationPosition(NiPoint3 point)
{
	gVirtualCameraState.position = point;
}

void CyCamera::SetLookAtSpeed(float fSpeed)
{
	gVirtualCameraState.lookAtSpeed = fSpeed;
}

void CyCamera::SetTargetDestination(NiPoint3 point)
{
	gVirtualCameraState.target = point;
}

void CyCamera::SetViewPortCenter(NiPoint2 pt2Center)
{
	gVirtualCameraState.viewportCenter = pt2Center;
}

void CyCamera::SetZoom(float zoom)
{
	gVirtualCameraState.zoom = zoom;
}

void CyCamera::SimpleLookAt(NiPoint3 position, NiPoint3 target)
{
	gVirtualCameraState.position = position;
	gVirtualCameraState.target = target;
}

void CyCamera::Translate(NiPoint3 translation)
{
	gVirtualCameraState.position = gVirtualCameraState.position + translation;
}

void CyCamera::ZoomIn(float increment)
{
	gVirtualCameraState.zoom += increment;
}

void CyCamera::ZoomOut(float increment)
{
	gVirtualCameraState.zoom -= increment;
}

bool CyCamera::isMoving()
{
	return false;
}

void CyCamera::setOrthoCamera(bool bNewValue)
{
	gVirtualCameraState.ortho = bNewValue;
}
