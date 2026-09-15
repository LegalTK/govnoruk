#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace Extrapolation
{
	inline bool Is_Finite(float Value)
	{
		std::uint32_t Bits;
		std::memcpy(&Bits, &Value, sizeof(Bits));
		return (Bits & 0x7f800000u) != 0x7f800000u;
	}

	inline bool Is_Finite(double Value)
	{
		std::uint64_t Bits;
		std::memcpy(&Bits, &Value, sizeof(Bits));
		return (Bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
	}

	struct Sample
	{
		double Time;
		int Receive_Tick;
		float Origin[3];
		float Velocity[3];
		int Flags;
		int Move_Type;
	};

	inline bool Is_Usable(const Sample& Record)
	{
		if (!Is_Finite(Record.Time) || Record.Time < 0. || Record.Receive_Tick < 0)
			return false;

		for (int Axis = 0; Axis < 3; ++Axis)
			if (!Is_Finite(Record.Origin[Axis]) || !Is_Finite(Record.Velocity[Axis]) ||
				std::abs(Record.Origin[Axis]) > 10000000.f || std::abs(Record.Velocity[Axis]) > 100000.f)
				return false;

		return true;
	}

	struct History
	{
		Sample Records[3]{};
		int Count = 0;

		void Reset()
		{
			Count = 0;
		}

		bool Push(const Sample& Record)
		{
			if (!Is_Usable(Record))
			{
				Reset();
				return false;
			}

			if (Count != 0)
			{
				const Sample& Previous = Records[Count - 1];
				const double Delta = Record.Time - Previous.Time;

				if (Record.Receive_Tick < Previous.Receive_Tick || Delta < 0. || Delta > 0.5)
					Reset();
				else if (Delta == 0.)
					return false;
				else
				{
					float Distance_Squared = 0.f;
					float Speed_Squared = 0.f;
					float Previous_Speed_Squared = 0.f;

					for (int Axis = 0; Axis < 3; ++Axis)
					{
						const float Difference = Record.Origin[Axis] - Previous.Origin[Axis];
						Distance_Squared += Difference * Difference;
						Speed_Squared += Record.Velocity[Axis] * Record.Velocity[Axis];
						Previous_Speed_Squared += Previous.Velocity[Axis] * Previous.Velocity[Axis];
					}

					const double Distance_Limit = (std::max)(64., 8. + 1.5 * Delta * std::sqrt((std::max)(Speed_Squared, Previous_Speed_Squared)));
					if (Distance_Squared > Distance_Limit * Distance_Limit || Record.Move_Type != Previous.Move_Type)
						Reset();
				}
			}

			if (Count == 3)
			{
				Records[0] = Records[1];
				Records[1] = Records[2];
				Count = 2;
			}

			Records[Count++] = Record;
			return true;
		}
	};

	inline int Get_Ticks(const Sample& Record, int Current_Tick, double Tick_Interval, double Outgoing_Latency)
	{
		if (!Is_Usable(Record) || !Is_Finite(Tick_Interval) || !Is_Finite(Outgoing_Latency) ||
			Tick_Interval < 1. / 4096. || Tick_Interval > 1. || Outgoing_Latency < 0. || Outgoing_Latency > 1.)
			return 0;

		const std::int64_t Receive_Age = static_cast<std::int64_t>(Current_Tick) - Record.Receive_Tick;
		if (Receive_Age < 0 || Receive_Age * Tick_Interval > 0.5)
			return 0;

		const double Simulation_Tick = Record.Time / Tick_Interval;
		if (Simulation_Tick > 2147483615. || Current_Tick > 2147483615)
			return 0;

		const double Ticks = Current_Tick + Outgoing_Latency / Tick_Interval - Record.Time / Tick_Interval;
		const int Limit = (std::min)(32, static_cast<int>(0.5 / Tick_Interval));

		if (!Is_Finite(Ticks) || Ticks < 0.5 || Ticks >= Limit + 0.5)
			return 0;

		return static_cast<int>(std::floor(Ticks + 0.5));
	}

	struct Trend
	{
		float Yaw_Rate = 0.f;
		float Acceleration = 0.f;
	};

	inline Trend Get_Trend(const History& Data)
	{
		Trend Result;
		if (Data.Count != 3)
			return Result;

		float Speeds[3];
		for (int Index = 0; Index < 3; ++Index)
		{
			const Sample& Record = Data.Records[Index];
			if (!Is_Usable(Record) || Record.Move_Type != 2 || ((Record.Flags ^ Data.Records[2].Flags) & 3) != 0)
				return Result;
			Speeds[Index] = std::hypot(Record.Velocity[0], Record.Velocity[1]);
			if (Speeds[Index] < 20.f)
				return Result;
		}

		float Yaw_Rates[2];
		float Accelerations[2];
		for (int Index = 0; Index < 2; ++Index)
		{
			const Sample& Previous = Data.Records[Index];
			const Sample& Current = Data.Records[Index + 1];
			const double Delta = Current.Time - Previous.Time;
			if (Delta < 1. / 4096. || Delta > 0.5)
				return Result;

			const float Cross = Previous.Velocity[0] * Current.Velocity[1] - Previous.Velocity[1] * Current.Velocity[0];
			const float Dot = Previous.Velocity[0] * Current.Velocity[0] + Previous.Velocity[1] * Current.Velocity[1];
			Yaw_Rates[Index] = static_cast<float>(std::atan2(Cross, Dot) / Delta);
			Accelerations[Index] = static_cast<float>((Speeds[Index + 1] - Speeds[Index]) / Delta);
		}

		if (Yaw_Rates[0] * Yaw_Rates[1] > 0.f && std::abs(Yaw_Rates[0]) <= 6.2831853f && std::abs(Yaw_Rates[1]) <= 6.2831853f &&
			std::abs(Yaw_Rates[1] - Yaw_Rates[0]) <= (std::max)(0.5f, std::abs(Yaw_Rates[1]) * 0.5f))
			Result.Yaw_Rate = (Yaw_Rates[0] + 2.f * Yaw_Rates[1]) / 3.f;

		if (Accelerations[0] * Accelerations[1] > 0.f &&
			std::abs(Accelerations[1] - Accelerations[0]) <= (std::max)(50.f, std::abs(Accelerations[1]) * 0.5f))
			Result.Acceleration = std::clamp((Accelerations[0] + 2.f * Accelerations[1]) / 3.f, -500.f, 500.f);

		return Result;
	}

	inline void Get_Desired_Move(const Sample& Record, const Trend& Motion, double Elapsed, float* Move)
	{
		const float Time = static_cast<float>(std::clamp(Elapsed, 0., 0.125));
		const float Speed = std::hypot(Record.Velocity[0], Record.Velocity[1]);
		const float Scale = Speed > 0.f ? (std::max)(0.f, Speed + Motion.Acceleration * Time) / Speed : 0.f;
		const float Angle = Motion.Yaw_Rate * Time;
		const float Cosine = std::cos(Angle);
		const float Sine = std::sin(Angle);
		Move[0] = (Record.Velocity[0] * Cosine - Record.Velocity[1] * Sine) * Scale;
		Move[1] = (Record.Velocity[0] * Sine + Record.Velocity[1] * Cosine) * Scale;
		Move[2] = Record.Velocity[2];
	}
}
