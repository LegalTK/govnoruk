#include "../SegaGmodX64/Extrapolation.hpp"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>

namespace
{
	struct Test_Runner
	{
		int Checks = 0;
		int Failures = 0;

		void Check(bool Condition, const std::string& Name)
		{
			++Checks;
			if (!Condition)
			{
				++Failures;
				std::cerr << "FAIL " << Name << '\n';
			}
		}
	};

	Extrapolation::Sample Make_Sample(double Time, int Receive_Tick, float X, float Y, float Z, float VX, float VY, float VZ, int Flags = 0, int Move_Type = 2)
	{
		Extrapolation::Sample Result{};
		Result.Time = Time;
		Result.Receive_Tick = Receive_Tick;
		Result.Origin[0] = X;
		Result.Origin[1] = Y;
		Result.Origin[2] = Z;
		Result.Velocity[0] = VX;
		Result.Velocity[1] = VY;
		Result.Velocity[2] = VZ;
		Result.Flags = Flags;
		Result.Move_Type = Move_Type;
		return Result;
	}

	bool Near(double Actual, double Expected, double Epsilon)
	{
		return Extrapolation::Is_Finite(Actual) && std::abs(Actual - Expected) <= Epsilon;
	}

	bool Near_Float(float Actual, float Expected, float Epsilon)
	{
		return Extrapolation::Is_Finite(Actual) && std::abs(Actual - Expected) <= Epsilon;
	}

	Extrapolation::History Make_Heading_History(const float* Angles, const float* Speeds, int Flags = 0)
	{
		Extrapolation::History Result;
		for (int Index = 0; Index < 3; ++Index)
		{
			const float Speed = Speeds[Index];
			const float Angle = Angles[Index];
			Result.Push(Make_Sample(Index * 0.1, Index, Index * 10.f, 0.f, 0.f, Speed * std::cos(Angle), Speed * std::sin(Angle), 0.f, Flags));
		}
		return Result;
	}

	void Test_Finite_And_Usable(Test_Runner& Tests)
	{
		const float Float_Nan = std::numeric_limits<float>::quiet_NaN();
		const float Float_Inf = std::numeric_limits<float>::infinity();
		const double Double_Nan = std::numeric_limits<double>::quiet_NaN();
		const double Double_Inf = std::numeric_limits<double>::infinity();
		Tests.Check(Extrapolation::Is_Finite(0.f), "finite float");
		Tests.Check(Extrapolation::Is_Finite(0.), "finite double");
		Tests.Check(!Extrapolation::Is_Finite(Float_Nan), "nonfinite float nan");
		Tests.Check(!Extrapolation::Is_Finite(Float_Inf), "nonfinite float infinity");
		Tests.Check(!Extrapolation::Is_Finite(Double_Nan), "nonfinite double nan");
		Tests.Check(!Extrapolation::Is_Finite(Double_Inf), "nonfinite double infinity");

		const Extrapolation::Sample Valid = Make_Sample(1.0, 4, 1.f, 2.f, 3.f, 4.f, 5.f, 6.f);
		Tests.Check(Extrapolation::Is_Usable(Valid), "ordinary sample usable");

		Extrapolation::Sample Invalid = Valid;
		Invalid.Time = Double_Nan;
		Tests.Check(!Extrapolation::Is_Usable(Invalid), "nan time unusable");
		Invalid = Valid;
		Invalid.Time = Double_Inf;
		Tests.Check(!Extrapolation::Is_Usable(Invalid), "infinite time unusable");
		Invalid = Valid;
		Invalid.Receive_Tick = -1;
		Tests.Check(!Extrapolation::Is_Usable(Invalid), "negative receive tick unusable");
		for (int Axis = 0; Axis < 3; ++Axis)
		{
			Invalid = Valid;
			Invalid.Origin[Axis] = Float_Nan;
			Tests.Check(!Extrapolation::Is_Usable(Invalid), "nan origin axis " + std::to_string(Axis));
			Invalid = Valid;
			Invalid.Velocity[Axis] = Float_Inf;
			Tests.Check(!Extrapolation::Is_Usable(Invalid), "infinite velocity axis " + std::to_string(Axis));
		}
		Invalid = Valid;
		Invalid.Origin[0] = 10000000.f;
		Tests.Check(Extrapolation::Is_Usable(Invalid), "origin exact magnitude bound usable");
		Invalid.Origin[0] = 10000001.f;
		Tests.Check(!Extrapolation::Is_Usable(Invalid), "origin beyond magnitude bound unusable");
		Invalid = Valid;
		Invalid.Velocity[1] = -100000.f;
		Tests.Check(Extrapolation::Is_Usable(Invalid), "velocity exact magnitude bound usable");
		Invalid.Velocity[1] = -100001.f;
		Tests.Check(!Extrapolation::Is_Usable(Invalid), "velocity beyond magnitude bound unusable");
	}

	void Test_Get_Ticks(Test_Runner& Tests)
	{
		const double Tick_Interval = 1. / 64.;
		const int Current_Tick = 100;
		Extrapolation::Sample Record = Make_Sample((100. - 3.75) * Tick_Interval, 99, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, Current_Tick, Tick_Interval, 0.5 * Tick_Interval) == 4, "ticks use simulation age plus outgoing latency");
		Record.Time = (100. - 0.75) * Tick_Interval;
		Record.Receive_Tick = Current_Tick;
		Tests.Check(Extrapolation::Get_Ticks(Record, Current_Tick, Tick_Interval, 0.) == 1, "fractional simulation age rounds once");

		const double Packet_Interval = 0.01;
		Record = Make_Sample((200. - 3.4) * Packet_Interval, 200, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 200, Packet_Interval, 0.2 * Packet_Interval) == 4, "packet gap is not rounded into batches");

		Record = Make_Sample((100. - 2.) * Tick_Interval, 67, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 0, "stale receive age rejected");
		Record.Receive_Tick = 101;
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 0, "future receive tick rejected");
		Record.Receive_Tick = 100;
		Record.Time = std::numeric_limits<double>::quiet_NaN();
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 0, "nonfinite record rejected");

		Record = Make_Sample(0., 100, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		const double Invalid_Intervals[] = {0., 1. / 8192., 1.000001, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
		for (int Index = 0; Index < 5; ++Index)
			Tests.Check(Extrapolation::Get_Ticks(Record, 100, Invalid_Intervals[Index], 0.) == 0, "invalid interval " + std::to_string(Index));
		const double Invalid_Latencies[] = {-0.0001, 1.0001, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()};
		for (int Index = 0; Index < 4; ++Index)
			Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, Invalid_Latencies[Index]) == 0, "invalid latency " + std::to_string(Index));

		const double Fine_Interval = 1. / 128.;
		Record = Make_Sample((500. - 32.) * Fine_Interval, 500, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 500, Fine_Interval, 0.) == 32, "32 tick upper bound inclusive");
		Record.Time = (500. - 32.5) * Fine_Interval;
		Tests.Check(Extrapolation::Get_Ticks(Record, 500, Fine_Interval, 0.) == 0, "32 tick half-step upper bound rejected");
		const double Coarse_Interval = 1. / 32.;
		Record = Make_Sample((100. - 16.) * Coarse_Interval, 100, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Coarse_Interval, 0.) == 16, "interval-specific 16 tick bound");
		Record.Time = (100. - 16.5) * Coarse_Interval;
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Coarse_Interval, 0.) == 0, "interval-specific upper bound rejected");
		Record = Make_Sample((100. - 2.) * Tick_Interval, 67, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 0, "500ms receive age bound rejected");
		Record.Receive_Tick = 68;
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 2, "500ms receive age equality accepted");
		Record = Make_Sample((100. - 21.) * Tick_Interval, 100, 0.f, 0.f, 0.f, 300.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 3. * Tick_Interval) == 24, "21 choked ticks plus latency supported");
		Record = Make_Sample((2147483647. - 1.) * Tick_Interval, 2147483647, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f);
		Tests.Check(Extrapolation::Get_Ticks(Record, 2147483647, Tick_Interval, 0.) == 0, "command tick overflow rejected");
		Record.Receive_Tick = 100;
		Record.Time = 0.;
		Tests.Check(Extrapolation::Get_Ticks(Record, 100, Tick_Interval, 0.) == 0, "excessive simulation age bound rejected");
	}

	void Test_History_Boundaries(Test_Runner& Tests)
	{
		Extrapolation::History History;
		const Extrapolation::Sample First = Make_Sample(1., 10, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f);
		Tests.Check(History.Push(First), "first history sample accepted");
		Tests.Check(!History.Push(First), "duplicate timestamp rejected");
		Tests.Check(History.Count == 1, "duplicate leaves history unchanged");
		Tests.Check(History.Push(Make_Sample(1.1, 11, 10.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "ordinary history sample accepted");

		Extrapolation::History Rewound;
		Rewound.Push(First);
		Tests.Check(Rewound.Push(Make_Sample(0.9, 11, 1.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "time rewind resets and accepts new sample");
		Tests.Check(Rewound.Count == 1 && Near(Rewound.Records[0].Time, 0.9, 0.), "time rewind retained only new sample");

		Extrapolation::History Receive_Rewound;
		Receive_Rewound.Push(First);
		Tests.Check(Receive_Rewound.Push(Make_Sample(1.1, 9, 1.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "receive rewind resets and accepts new sample");
		Tests.Check(Receive_Rewound.Count == 1, "receive rewind retained one sample");

		Extrapolation::History Gap;
		Gap.Push(First);
		Tests.Check(Gap.Push(Make_Sample(1.501, 11, 1.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "large packet time gap resets and accepts");
		Tests.Check(Gap.Count == 1, "large packet gap retained one sample");

		Extrapolation::History Teleport;
		Teleport.Push(First);
		Tests.Check(Teleport.Push(Make_Sample(1.1, 11, 1000.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "teleport resets and accepts");
		Tests.Check(Teleport.Count == 1, "teleport retained one sample");

		Extrapolation::History Move_Type;
		Move_Type.Push(First);
		Tests.Check(Move_Type.Push(Make_Sample(1.1, 11, 1.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0, 1)), "move type change resets and accepts");
		Tests.Check(Move_Type.Count == 1, "move type change retained one sample");

		Extrapolation::History Invalid;
		Invalid.Push(First);
		Extrapolation::Sample Invalid_Sample = Make_Sample(1.1, 11, 1.f, 0.f, 0.f, 100.f, 0.f, 0.f);
		Invalid_Sample.Velocity[1] = std::numeric_limits<float>::quiet_NaN();
		Tests.Check(!Invalid.Push(Invalid_Sample), "invalid sample rejected");
		Tests.Check(Invalid.Count == 0, "invalid sample resets history");

		Extrapolation::History Rolling;
		for (int Index = 0; Index < 4; ++Index)
			Tests.Check(Rolling.Push(Make_Sample(Index * 0.1, Index, Index * 10.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "rolling sample " + std::to_string(Index));
		Tests.Check(Rolling.Count == 3, "history stores three samples");
		Tests.Check(Near(Rolling.Records[0].Time, 0.1, 1e-12) && Near(Rolling.Records[1].Time, 0.2, 1e-12) && Near(Rolling.Records[2].Time, 0.3, 1e-12), "history rolls oldest sample");

		Extrapolation::History Packet_Gap;
		Tests.Check(Packet_Gap.Push(Make_Sample(1.000, 20, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f)), "subtick packet sample 0");
		Tests.Check(Packet_Gap.Push(Make_Sample(1.013, 24, 1.3f, 0.f, 0.f, 100.f, 0.f, 0.f)), "subtick packet sample 1");
		Tests.Check(Packet_Gap.Push(Make_Sample(1.026, 28, 2.6f, 0.f, 0.f, 100.f, 0.f, 0.f)), "subtick packet sample 2");
		Tests.Check(Packet_Gap.Count == 3 && Near(Packet_Gap.Records[1].Time, 1.013, 1e-12), "subtick packet times stay unbatched");
	}

	void Test_Trends(Test_Runner& Tests)
	{
		const float Stable_Counterclockwise_Angles[] = {3.10f, 3.20f, 3.30f};
		const float Stable_Speeds[] = {100.f, 100.f, 100.f};
		const Extrapolation::History Counterclockwise = Make_Heading_History(Stable_Counterclockwise_Angles, Stable_Speeds);
		const Extrapolation::Trend Counterclockwise_Trend = Extrapolation::Get_Trend(Counterclockwise);
		Tests.Check(Near_Float(Counterclockwise_Trend.Yaw_Rate, 1.f, 0.0002f), "stable counterclockwise turn across heading wrap");
		Tests.Check(Counterclockwise_Trend.Acceleration == 0.f, "constant speed has zero acceleration");

		const float Stable_Clockwise_Angles[] = {-3.10f, -3.20f, -3.30f};
		const Extrapolation::History Clockwise = Make_Heading_History(Stable_Clockwise_Angles, Stable_Speeds);
		const Extrapolation::Trend Clockwise_Trend = Extrapolation::Get_Trend(Clockwise);
		Tests.Check(Near_Float(Clockwise_Trend.Yaw_Rate, -1.f, 0.0002f), "stable clockwise turn across heading wrap");

		const float Reversing_Angles[] = {0.f, 0.1f, 0.f};
		const Extrapolation::Trend Reversing_Trend = Extrapolation::Get_Trend(Make_Heading_History(Reversing_Angles, Stable_Speeds));
		Tests.Check(Reversing_Trend.Yaw_Rate == 0.f, "reversing turn rejected");
		const float Inconsistent_Angles[] = {0.f, 0.1f, 0.4f};
		const Extrapolation::Trend Inconsistent_Trend = Extrapolation::Get_Trend(Make_Heading_History(Inconsistent_Angles, Stable_Speeds));
		Tests.Check(Inconsistent_Trend.Yaw_Rate == 0.f, "inconsistent turn rejected");

		const float Stop_Speeds[] = {100.f, 19.99f, 100.f};
		const Extrapolation::Trend Stop_Trend = Extrapolation::Get_Trend(Make_Heading_History(Stable_Clockwise_Angles, Stop_Speeds));
		Tests.Check(Stop_Trend.Yaw_Rate == 0.f && Stop_Trend.Acceleration == 0.f, "stop below speed threshold rejects trend");

		const float Straight_Angles[] = {0.f, 0.f, 0.f};
		const float Accelerating_Speeds[] = {100.f, 110.f, 120.f};
		const Extrapolation::Trend Acceleration_Trend = Extrapolation::Get_Trend(Make_Heading_History(Straight_Angles, Accelerating_Speeds));
		Tests.Check(Near_Float(Acceleration_Trend.Acceleration, 100.f, 0.001f), "stable acceleration estimated");
		const float Hard_Accelerating_Speeds[] = {100.f, 180.f, 260.f};
		const Extrapolation::Trend Hard_Acceleration_Trend = Extrapolation::Get_Trend(Make_Heading_History(Straight_Angles, Hard_Accelerating_Speeds));
		Tests.Check(Hard_Acceleration_Trend.Acceleration == 500.f, "positive acceleration capped");
		const float Braking_Speeds[] = {300.f, 290.f, 280.f};
		const Extrapolation::Trend Braking_Trend = Extrapolation::Get_Trend(Make_Heading_History(Straight_Angles, Braking_Speeds));
		Tests.Check(Near_Float(Braking_Trend.Acceleration, -100.f, 0.001f), "stable braking estimated");
		const float Hard_Braking_Speeds[] = {260.f, 180.f, 100.f};
		const Extrapolation::Trend Hard_Braking_Trend = Extrapolation::Get_Trend(Make_Heading_History(Straight_Angles, Hard_Braking_Speeds));
		Tests.Check(Hard_Braking_Trend.Acceleration == -500.f, "negative acceleration capped");
		const float Inconsistent_Speeds[] = {100.f, 101.f, 111.f};
		const Extrapolation::Trend Inconsistent_Acceleration = Extrapolation::Get_Trend(Make_Heading_History(Straight_Angles, Inconsistent_Speeds));
		Tests.Check(Inconsistent_Acceleration.Acceleration == 0.f, "inconsistent acceleration rejected");

		Extrapolation::History Ground_Air;
		Ground_Air.Push(Make_Sample(0., 0, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0));
		Ground_Air.Push(Make_Sample(0.1, 1, 10.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0));
		Ground_Air.Push(Make_Sample(0.2, 2, 20.f, 0.f, 0.f, 100.f, 0.f, 0.f, 1));
		const Extrapolation::Trend Ground_Air_Trend = Extrapolation::Get_Trend(Ground_Air);
		Tests.Check(Ground_Air.Count == 3, "ground-air history remains available");
		Tests.Check(Ground_Air_Trend.Yaw_Rate == 0.f && Ground_Air_Trend.Acceleration == 0.f, "ground-air flag change rejects trend");
		Extrapolation::History Other_Flag;
		Other_Flag.Push(Make_Sample(0., 0, 0.f, 0.f, 0.f, 100.f, 0.f, 0.f, 4));
		Other_Flag.Push(Make_Sample(0.1, 1, 10.f, 0.f, 0.f, 100.f, 0.f, 0.f, 4));
		Other_Flag.Push(Make_Sample(0.2, 2, 20.f, 0.f, 0.f, 100.f, 0.f, 0.f, 0));
		Tests.Check(Extrapolation::Get_Trend(Other_Flag).Acceleration == 0.f, "unrelated flags do not reject trend");

		Extrapolation::History Choked_Turn;
		for (int Index = 0; Index < 3; ++Index)
			Choked_Turn.Push(Make_Sample(Index * 0.3, Index * 20, Index * 30.f, 0.f, 0.f, 100.f * std::cos(Index * 0.3f), 100.f * std::sin(Index * 0.3f), 0.f));
		Tests.Check(Near_Float(Extrapolation::Get_Trend(Choked_Turn).Yaw_Rate, 1.f, 0.0002f), "stable turn survives 300ms packet gaps");
		Choked_Turn.Push(Make_Sample(1.101, 80, 90.f, 0.f, 0.f, 100.f, 0.f, 0.f));
		Tests.Check(Choked_Turn.Count == 1 && Extrapolation::Get_Trend(Choked_Turn).Yaw_Rate == 0.f, "trend resets after gaps over 500ms");
	}

	void Test_Desired_Move(Test_Runner& Tests)
	{
		const Extrapolation::Sample Record = Make_Sample(0., 0, 0.f, 0.f, 0.f, 100.f, 0.f, 7.f);
		Extrapolation::Trend Motion;
		Motion.Yaw_Rate = 2.f;
		Motion.Acceleration = 100.f;
		float Move[3]{};
		Extrapolation::Get_Desired_Move(Record, Motion, 0.125, Move);
		float Capped[3]{};
		Extrapolation::Get_Desired_Move(Record, Motion, 0.5, Capped);
		Tests.Check(Near_Float(Move[0], Capped[0], 0.0001f) && Near_Float(Move[1], Capped[1], 0.0001f) && Near_Float(Move[2], Capped[2], 0.f), "elapsed horizon caps at 125ms");
		Tests.Check(Near_Float(Move[0], 112.5f * std::cos(0.25f), 0.0002f) && Near_Float(Move[1], 112.5f * std::sin(0.25f), 0.0002f), "desired move applies turn and acceleration");
		Extrapolation::Get_Desired_Move(Record, Motion, -1., Move);
		Tests.Check(Near_Float(Move[0], 100.f, 0.0001f) && Near_Float(Move[1], 0.f, 0.0001f), "negative elapsed clamps to zero");
		Motion.Acceleration = -1000.f;
		Extrapolation::Get_Desired_Move(Record, Motion, 0.125, Move);
		Tests.Check(Move[0] == 0.f && Move[1] == 0.f && Move[2] == 7.f, "deceleration floors horizontal speed at zero");
		const Extrapolation::Sample Zero_Horizontal = Make_Sample(0., 0, 0.f, 0.f, 0.f, 0.f, 0.f, 4.f);
		Motion.Yaw_Rate = 1.f;
		Motion.Acceleration = 100.f;
		Extrapolation::Get_Desired_Move(Zero_Horizontal, Motion, 0.1, Move);
		Tests.Check(Move[0] == 0.f && Move[1] == 0.f && Move[2] == 4.f, "zero horizontal speed stays zero");
	}

	void Test_Randomized_Invariants(Test_Runner& Tests)
	{
		std::mt19937 Generator(0x51e9a7u);
		std::uniform_real_distribution<float> Speed(20.f, 200.f);
		std::uniform_real_distribution<float> Angle(-3.1415926f, 3.1415926f);
		std::uniform_real_distribution<double> Age(0., 31.9);
		std::uniform_real_distribution<double> Latency(0., 0.2);
		const double Intervals[] = {1. / 256., 1. / 128., 1. / 64., 0.01};
		for (int Index = 0; Index < 600; ++Index)
		{
			const double Interval = Intervals[Index % 4];
			const int Current_Tick = 1000;
			const double Simulation_Age = Age(Generator);
			Extrapolation::Sample Record = Make_Sample((Current_Tick - Simulation_Age) * Interval, Current_Tick, 0.f, 0.f, 0.f, Speed(Generator) * std::cos(Angle(Generator)), Speed(Generator) * std::sin(Angle(Generator)), 0.f);
			const int Ticks = Extrapolation::Get_Ticks(Record, Current_Tick, Interval, Latency(Generator));
			Tests.Check(Ticks >= 0 && Ticks <= 32, "random ticks range " + std::to_string(Index));
		}

		Extrapolation::History History;
		float X = 0.f;
		float Y = 0.f;
		int Receive_Tick = 300;
		for (int Index = 0; Index < 300; ++Index)
		{
			const float Speed_Value = Speed(Generator);
			const float Angle_Value = Angle(Generator);
			const float VX = Speed_Value * std::cos(Angle_Value);
			const float VY = Speed_Value * std::sin(Angle_Value);
			const Extrapolation::Sample Record = Make_Sample(Index * 0.01, Receive_Tick, X, Y, 0.f, VX, VY, 0.f);
			Tests.Check(History.Push(Record), "random history push " + std::to_string(Index));
			X += VX * 0.01f;
			Y += VY * 0.01f;
			Receive_Tick += Index % 3 == 0 ? 0 : 1;
			if (History.Count == 3)
			{
				const Extrapolation::Trend Trend = Extrapolation::Get_Trend(History);
				Tests.Check(Extrapolation::Is_Finite(Trend.Yaw_Rate) && Extrapolation::Is_Finite(Trend.Acceleration), "random trend finite " + std::to_string(Index));
			}
		}
	}
}

int main()
{
	Test_Runner Tests;
	Test_Finite_And_Usable(Tests);
	Test_Get_Ticks(Tests);
	Test_History_Boundaries(Tests);
	Test_Trends(Tests);
	Test_Desired_Move(Tests);
	Test_Randomized_Invariants(Tests);
	std::cout << "CHECKS " << Tests.Checks << " FAILURES " << Tests.Failures << '\n';
	return Tests.Failures == 0 ? 0 : 1;
}
