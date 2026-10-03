#pragma once

namespace RACE::CameraMath
{
	inline constexpr float DEG2RAD = std::numbers::pi_v<float> / 180.0f;

	// Framerate independent lerp factor
	inline float SmoothFactor(float a_speed, float a_dt)
	{
		return 1.0f - std::exp(-a_speed * a_dt);
	}

	// Returns the step to apply and removes it from a_pending, settling to exactly 0
	inline float DrainSmoothed(float& a_pending, float a_factor, float a_epsilon)
	{
		float step = std::abs(a_pending) > a_epsilon ? a_pending * a_factor : a_pending;
		a_pending -= step;
		return step;
	}

	inline constexpr float MIN_FOV = 1.0f;
	inline constexpr float MAX_FOV = 170.0f;

	inline float ClampFov(float a_fov) { return std::clamp(a_fov, MIN_FOV, MAX_FOV); }
	inline float TanHalf(float a_fovDeg) { return std::tan(a_fovDeg * 0.5f * DEG2RAD); }
	inline float FovFromTanHalf(float a_tanHalf) { return 2.0f * std::atan(a_tanHalf) / DEG2RAD; }

	inline float FovScale(float a_baseFov, float a_fov)
	{
		float scale = TanHalf(ClampFov(a_fov)) / TanHalf(a_baseFov);
		return std::clamp(scale, 0.1f, 10.0f);
	}

	// Yaw Matrix (Rotation around world Z-axis / Up vector)
	inline RE::NiMatrix3 MakeYaw(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = c;
		m.entry[0][1] = -s;
		m.entry[0][2] = 0.0f;
		m.entry[1][0] = s;
		m.entry[1][1] = c;
		m.entry[1][2] = 0.0f;
		m.entry[2][0] = 0.0f;
		m.entry[2][1] = 0.0f;
		m.entry[2][2] = 1.0f;
		return m;
	}

	// Pitch Matrix (Rotation around local X-axis / Right vector)
	inline RE::NiMatrix3 MakePitch(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = 1.0f;
		m.entry[0][1] = 0.0f;
		m.entry[0][2] = 0.0f;
		m.entry[1][0] = 0.0f;
		m.entry[1][1] = c;
		m.entry[1][2] = -s;
		m.entry[2][0] = 0.0f;
		m.entry[2][1] = s;
		m.entry[2][2] = c;
		return m;
	}

	// Roll Matrix (Rotation around local Y-axis / Forward vector)
	inline RE::NiMatrix3 MakeRoll(float a_angle)
	{
		float c = std::cos(a_angle);
		float s = std::sin(a_angle);

		RE::NiMatrix3 m;
		m.entry[0][0] = c;
		m.entry[0][1] = 0.0f;
		m.entry[0][2] = -s;
		m.entry[1][0] = 0.0f;
		m.entry[1][1] = 1.0f;
		m.entry[1][2] = 0.0f;
		m.entry[2][0] = s;
		m.entry[2][1] = 0.0f;
		m.entry[2][2] = c;
		return m;
	}

	// Camera axes: X right, Y forward, Z up
	inline RE::NiPoint3 RightAxis(const RE::NiMatrix3& a_rot) { return { a_rot.entry[0][0], a_rot.entry[1][0], a_rot.entry[2][0] }; }
	inline RE::NiPoint3 ForwardAxis(const RE::NiMatrix3& a_rot) { return { a_rot.entry[0][1], a_rot.entry[1][1], a_rot.entry[2][1] }; }

	inline RE::NiPoint3 OrbitAroundZ(const RE::NiPoint3& a_pivot, const RE::NiPoint3& a_point, float a_yaw)
	{
		float c = std::cos(a_yaw);
		float s = std::sin(a_yaw);

		float rx = a_point.x - a_pivot.x;
		float ry = a_point.y - a_pivot.y;

		return {
			a_pivot.x + (rx * c - ry * s),
			a_pivot.y + (rx * s + ry * c),
			a_point.z
		};
	}

	struct BodyFraming
	{
		RE::NiPoint3 camPos;
		float        depth;
	};

	inline std::optional<BodyFraming> FrameFullBody(
		const RE::NiPoint3&  a_pelvisPos,
		const RE::NiPoint3&  a_rootPos,
		const RE::NiMatrix3& a_camRotate,
		float                a_tanHalfV,
		float                a_aspect,
		float                a_margin,
		float                a_screenX)
	{
		// Pelvis to root (floor) is the longer half; margin covers the top of the head
		float halfHeight = (a_pelvisPos - a_rootPos).Length() * a_margin;
		if (halfHeight < 1.0f)
			return std::nullopt;

		const RE::NiPoint3 right = RightAxis(a_camRotate);
		const RE::NiPoint3 fwd   = ForwardAxis(a_camRotate);

		float tanH  = a_tanHalfV * a_aspect;
		float depth = halfHeight / a_tanHalfV;

		RE::NiPoint3 camPos = a_pelvisPos - fwd * depth - right * (a_screenX * depth * tanH);
		return BodyFraming{ camPos, depth };
	}

	inline float ApplyDeadzone(float a_value, float a_deadzone)
	{
		float magnitude = std::abs(a_value);
		if (magnitude < a_deadzone)
			return 0.0f;

		float normalized = (magnitude - a_deadzone) / (1.0f - a_deadzone);
		float curved     = normalized * normalized;
		return a_value < 0.0f ? -curved : curved;
	}
}
