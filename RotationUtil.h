#pragma once
#include "Vector3.h"

// Every rotation in this engine is stored as a Vector3 of Euler angles and
// ends up in XMMatrixRotationRollPitchYaw(rotation.x, rotation.y, rotation.z)
// (see GameObject::GetMatrx). There, x is the angle around X, y around Y and
// z around Z, and DirectXMath composes them in the order Z -> X -> Y.
//
// So a matrix or quaternion has to be taken apart in exactly THAT order to
// survive the round trip. The textbook "quaternion to euler" snippet that
// floats around the web decomposes in the aerospace Z -> Y -> X order and
// names its three results roll / pitch / yaw after the X / Y / Z axes.
// DirectXMath also says "roll pitch yaw", but means pitch = X, yaw = Y,
// roll = Z - matching those up by NAME instead of by AXIS both scrambles the
// axes and uses the wrong composition order, which turns any rotation that
// is not around a single axis into garbage.
inline Vector3 EulerFromRotationMatrix(const XMMATRIX& Matrix)
{
	// Normalize the basis rows so a matrix that still carries scale
	// (a bone matrix usually does) decomposes the same as a pure rotation.
	XMFLOAT3 rowX, rowY, rowZ;
	XMStoreFloat3(&rowX, XMVector3Normalize(Matrix.r[0]));
	XMStoreFloat3(&rowY, XMVector3Normalize(Matrix.r[1]));
	XMStoreFloat3(&rowZ, XMVector3Normalize(Matrix.r[2]));

	// With M = Rz(z) * Rx(x) * Ry(y) (row-vector convention):
	//   rowZ.y = -sin(x)
	//   rowZ.x =  cos(x)sin(y)   rowZ.z = cos(x)cos(y)
	//   rowX.y =  sin(z)cos(x)   rowY.y = cos(z)cos(x)
	float sinX = -rowZ.y;
	sinX = (sinX < -1.0f) ? -1.0f : ((sinX > 1.0f) ? 1.0f : sinX);

	Vector3 euler;
	euler.x = asinf(sinX);

	if (fabsf(rowZ.y) < 0.999999f)
	{
		euler.y = atan2f(rowZ.x, rowZ.z);
		euler.z = atan2f(rowX.y, rowY.y);
	}
	else
	{
		// Gimbal lock: X is +-90 degrees and only (z - y) is defined, so
		// pin the roll at 0 and put the whole remaining rotation into yaw.
		euler.y = atan2f(-rowX.z, rowX.x);
		euler.z = 0.0f;
	}

	return euler;
}

inline Vector3 EulerFromQuaternion(XMVECTOR Quaternion)
{
	return EulerFromRotationMatrix(XMMatrixRotationQuaternion(Quaternion));
}
