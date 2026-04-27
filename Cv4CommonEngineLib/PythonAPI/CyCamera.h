#pragma once

#include <CvGameCoreDLL/CommonShared.h>
#include <CvGameCoreDLL/CvEnums.h>

#include <pybind11/pybind11.h>

class CyPlot;
class CyUnit;

class CyCamera
{
public:
	static void registerWithPython(const pybind11::module& m);

	float GetBasePitch();
	float GetBaseTurn();
	float GetCameraMovementSpeed();
	NiPoint3 GetCurrentPosition();
	NiPoint2 GetDefaultViewPortCenter();
	NiPoint3 GetDestinationPosition();
	// fortsnek; What do these do?
	//void GetLookAt(NiPoint3 pt3LookAt);
	//float GetLookAtSpeed();
	NiPoint3 GetTargetDestination();
	float GetZoom();
	void JustLookAt(NiPoint3 p3LookAt);
	void JustLookAtPlot(CyPlot pPlot);
	void LookAt(NiPoint3 pt3LookAt, CameraLookAtTypes type, NiPoint3 attackDirection);
	void LookAtUnit(CyUnit unit);
	void MoveBaseTurnLeft(float increment);
	void MoveBaseTurnRight(float increment);
	void ReleaseLockedCamera();
	void ResetZoom();
	void SetBasePitch(float fBasePitch);
	void SetBaseTurn(float baseTurn);
	void SetCameraMovementSpeed(CameraMovementSpeeds eSpeed);
	void SetCurrentPosition(NiPoint3 point);
	void SetDestinationPosition(NiPoint3 point);
	void SetLookAtSpeed(float fSpeed);
	void SetTargetDestination(NiPoint3 point);
	void SetViewPortCenter(NiPoint2 pt2Center);
	void SetZoom(float zoom);
	void SimpleLookAt(NiPoint3 position, NiPoint3 target);
	void Translate(NiPoint3 translation);
	void ZoomIn(float increment);
	void ZoomOut(float increment);
	bool isMoving();
	void setOrthoCamera(bool bNewValue);
};
