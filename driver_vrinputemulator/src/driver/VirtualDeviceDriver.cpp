#include "VirtualDeviceDriver.h"

#include "../logging.h"


namespace vrinputemulator {
namespace driver {


VirtualDeviceDriver::VirtualDeviceDriver(ServerDriver* parent, VirtualDeviceType type, const std::string& serial, uint32_t virtualId)
		: m_serverDriver(parent), m_deviceType(type), m_serialNumber(serial), m_virtualDeviceId(virtualId) {
	memset(&m_pose, 0, sizeof(vr::DriverPose_t));
	m_pose.qDriverFromHeadRotation.w = 1;
	m_pose.qWorldFromDriverRotation.w = 1;
	m_pose.qRotation.w = 1;
	m_pose.result = vr::ETrackingResult::TrackingResult_Uninitialized;
	memset(&m_ControllerState, 0, sizeof(vr::VRControllerState_t));
}

vr::EVRInitError VirtualDeviceDriver::Activate(uint32_t unObjectId) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::Activate( " << unObjectId << " )";
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	m_propertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer(unObjectId);
	m_openvrId = unObjectId;
	//m_serverDriver->_trackedDeviceActivated(m_openvrId, this);
	for (auto& e : _deviceProperties) {
		auto errorMessage = boost::apply_visitor(DevicePropertyValueVisitor(m_propertyContainer, (vr::ETrackedDeviceProperty)e.first), e.second);
		if (!errorMessage.empty()) {
			LOG(ERROR) << "Could not set tracked device property: " << errorMessage;
		}
	}
	return vr::VRInitError_None;
}


void VirtualDeviceDriver::Deactivate() {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::Deactivate()";
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	//m_serverDriver->_trackedDeviceDeactivated(m_openvrId);
	m_openvrId = vr::k_unTrackedDeviceIndexInvalid;
}


void * VirtualDeviceDriver::GetComponent(const char * pchComponentNameAndVersion) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::GetComponent( " << pchComponentNameAndVersion << " )";
	if (std::strcmp(pchComponentNameAndVersion, vr::ITrackedDeviceServerDriver_Version) == 0) {
		return static_cast<vr::ITrackedDeviceServerDriver*>(this);
	} else if (std::strcmp(pchComponentNameAndVersion, vr::IVRDriverInput_Version) == 0) {
		return static_cast<vr::IVRDriverInput*>(this);
	}
	return nullptr;
}


vr::DriverPose_t VirtualDeviceDriver::GetPose() {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::GetPose()";
	return m_pose;
}


void VirtualDeviceDriver::updatePose(const vr::DriverPose_t & newPose, double timeOffset, bool notify) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::updatePose( " << timeOffset << " )";
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	m_pose = newPose;
	m_pose.poseTimeOffset += timeOffset;
	if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
		vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_pose, sizeof(vr::DriverPose_t));
	}
}

void VirtualDeviceDriver::sendPoseUpdate(double timeOffset, bool onlyWhenConnected) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::sendPoseUpdate( " << timeOffset << " )";
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	if (!onlyWhenConnected || (m_pose.poseIsValid && m_pose.deviceIsConnected)) {
		m_pose.poseTimeOffset = timeOffset;
		if (m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_pose, sizeof(vr::DriverPose_t));
		}
	}
}

void VirtualDeviceDriver::publish() {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::publish()";
	if (!m_published) {
		vr::ETrackedPropertyError pError;
		vr::ETrackedDeviceClass deviceClass = (vr::ETrackedDeviceClass)getTrackedDeviceProperty<int32_t>(vr::Prop_DeviceClass_Int32, &pError);
		if (pError == vr::TrackedProp_Success) {
			vr::VRServerDriverHost()->TrackedDeviceAdded(m_serialNumber.c_str(), deviceClass, this);
			m_published = true;
		} else {
			throw std::runtime_error(std::string("Could not get device class (") + std::to_string((int)pError) + std::string(")"));
		}
	}
}

vr::VRControllerState_t VirtualDeviceDriver::GetControllerState() {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::GetControllerState()";
	return m_ControllerState;
}

bool VirtualDeviceDriver::TriggerHapticPulse(uint32_t unAxisId, uint16_t usPulseDurationMicroseconds) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::TriggerHapticPulse( " << unAxisId << " )";
	return true; // returning false will cause errors to come out of vrserver
}


vr::EVRInputError CreateBooleanComponent(
		vr::PropertyContainerHandle_t ulContainer,
		const char* pchName,
		vr::VRInputComponentHandle_t* pHandle) {

	return vr::VRInputError_None;
}

vr::EVRInputError UpdateBooleanComponent(
		vr::VRInputComponentHandle_t ulComponent,
		bool bNewValue,
		double fTimeOffset) {

	return vr::VRInputError_None;
}

vr::EVRInputError CreateScalarComponent(
		vr::PropertyContainerHandle_t ulContainer,
		const char* pchName,
		vr::VRInputComponentHandle_t* pHandle,
		vr::EVRScalarType eType, vr::EVRScalarUnits eUnits) {

	return vr::VRInputError_None;
}

vr::EVRInputError UpdateScalarComponent(
		vr::VRInputComponentHandle_t ulComponent,
		float fNewValue,
		double fTimeOffset) {

	return vr::VRInputError_None;
}

vr::EVRInputError CreateHapticComponent(
		vr::PropertyContainerHandle_t ulContainer,
		const char* pchName,
		vr::VRInputComponentHandle_t* pHandle) {

	return vr::VRInputError_None;
}

vr::EVRInputError CreateSkeletonComponent(
		vr::PropertyContainerHandle_t ulContainer,
		const char* pchName,
		const char* pchSkeletonPath,
		const char* pchBasePosePath,
		vr::EVRSkeletalTrackingLevel eSkeletalTrackingLevel,
		const vr::VRBoneTransform_t* pGripLimitTransforms,
		uint32_t unGripLimitTransformCount,
		vr::VRInputComponentHandle_t* pHandle) {

	return vr::VRInputError_None;
}

vr::EVRInputError UpdateSkeletonComponent(
		vr::VRInputComponentHandle_t ulComponent,
		vr::EVRSkeletalMotionRange eMotionRange,
		const vr::VRBoneTransform_t* pTransforms,
		uint32_t unTransformCount) {

	return vr::VRInputError_None;
}


void VirtualDeviceDriver::updateControllerState(const vr::VRControllerState_t & newState, double offset, bool notify) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::updateControllerState()";
	std::lock_guard<std::recursive_mutex> lock(_mutex);
	if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
		auto oldState = m_ControllerState;
		m_ControllerState = newState;

		vr::DriverPose_t m_Pose; //TODO : ���Ή�

		// Iterate over buttons and check for state changes
		for (unsigned i = 0; i < vr::k_EButton_Max; ++i) {
			auto oldTouchedState = oldState.ulButtonTouched & vr::ButtonMaskFromId((vr::EVRButtonId)i);
			auto newTouchedState = newState.ulButtonTouched & vr::ButtonMaskFromId((vr::EVRButtonId)i);
			if (!oldTouchedState && newTouchedState) {
				LOG(DEBUG) << m_serialNumber << ": ButtonTouch detected: " << i;
				//vr::VRServerDriverHost()->TrackedDeviceButtonTouched(m_openvrId, (vr::EVRButtonId)i, offset);
				vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
			} else if (oldTouchedState && !newTouchedState) {
				LOG(DEBUG) << m_serialNumber << ": ButtonUntouch detected: " << i;
				//vr::VRServerDriverHost()->TrackedDeviceButtonUntouched(m_openvrId, (vr::EVRButtonId)i, offset);
				vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
			}
			auto oldPressedState = oldState.ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)i);
			auto newPressedState = newState.ulButtonPressed & vr::ButtonMaskFromId((vr::EVRButtonId)i);
			if (!oldPressedState && newPressedState) {
				LOG(DEBUG) << m_serialNumber << ": ButtonPress detected: " << i;
				//vr::VRServerDriverHost()->TrackedDeviceButtonPressed(m_openvrId, (vr::EVRButtonId)i, offset);
				vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
			} else if (oldPressedState && !newPressedState) {
				LOG(DEBUG) << m_serialNumber << ": ButtonUnpress detected: " << i;
				//vr::VRServerDriverHost()->TrackedDeviceButtonUnpressed(m_openvrId, (vr::EVRButtonId)i, offset);
				vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
			}
		}
		// Iterate over axis and check for state changes
		for (unsigned i = 0; i < vr::k_unControllerStateAxisCount; ++i) {
			if (oldState.rAxis[i].x != newState.rAxis[i].x || oldState.rAxis[i].y != newState.rAxis[i].y) {
				LOG(DEBUG) << m_serialNumber << ": AxisChange detected: " << i;
				//vr::VRServerDriverHost()->TrackedDeviceAxisUpdated(m_openvrId, i, newState.rAxis[i]);
				vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
			}
		}
	} else {
		m_ControllerState = newState;
	}
}

void VirtualDeviceDriver::buttonEvent(ButtonEventType eventType, uint32_t buttonId, double timeOffset, bool notify) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::buttonEvent( " << (int)eventType << ", " << buttonId << ", " << timeOffset << " )";

	vr::DriverPose_t m_Pose; //TODO : ���Ή�

	switch (eventType) {
	case ButtonEventType::ButtonPressed:
		m_ControllerState.ulButtonPressed |= vr::ButtonMaskFromId((vr::EVRButtonId)buttonId);
		if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			//vr::VRServerDriverHost()->TrackedDeviceButtonPressed(m_openvrId, (vr::EVRButtonId)buttonId, timeOffset);
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
		}
		break;
	case ButtonEventType::ButtonUnpressed:
		m_ControllerState.ulButtonPressed &= ~vr::ButtonMaskFromId((vr::EVRButtonId)buttonId);
		if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			//vr::VRServerDriverHost()->TrackedDeviceButtonUnpressed(m_openvrId, (vr::EVRButtonId)buttonId, timeOffset);
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
		}
		break;
	case ButtonEventType::ButtonTouched:
		m_ControllerState.ulButtonTouched |= vr::ButtonMaskFromId((vr::EVRButtonId)buttonId);
		if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			//vr::VRServerDriverHost()->TrackedDeviceButtonTouched(m_openvrId, (vr::EVRButtonId)buttonId, timeOffset);
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
		}
		break;
	case ButtonEventType::ButtonUntouched:
		m_ControllerState.ulButtonTouched &= ~vr::ButtonMaskFromId((vr::EVRButtonId)buttonId);
		if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			//vr::VRServerDriverHost()->TrackedDeviceButtonUntouched(m_openvrId, (vr::EVRButtonId)buttonId, timeOffset);
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
		}
		break;
	default:
		break;
	}
}

void VirtualDeviceDriver::axisEvent(uint32_t axisId, const vr::VRControllerAxis_t & axisState, bool notify) {
	LOG(TRACE) << "VirtualDeviceDriver[" << m_serialNumber << "]::axisEvent( " << axisId << " )";
	if (axisId < vr::k_unControllerStateAxisCount) {
		m_ControllerState.rAxis[axisId] = axisState;
		vr::DriverPose_t m_Pose; //TODO : ���Ή�

		if (notify && m_openvrId != vr::k_unTrackedDeviceIndexInvalid) {
			//vr::VRServerDriverHost()->TrackedDeviceAxisUpdated(m_openvrId, axisId, m_ControllerState.rAxis[axisId]);
			vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_openvrId, m_Pose, sizeof(vr::DriverPose_t));
		}
	}
}


} // end namespace driver
} // end namespace vrinputemulator
