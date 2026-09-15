#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <limits>
#include <vector>

#define __int8 char
#define __int16 short
#define __int32 int
#define __int64 long long

struct Global_Variables_Structure;
Global_Variables_Structure* Test_Globals;
void* Engine_Module;
void* Client_Module;
int Native_Updates;

struct Interface_Structure
{
	char* String;
};

namespace Byte_Manager
{
	template <typename... Arguments>
	void* Find_Bytes(Arguments...)
	{
		return nullptr;
	}

	void* Solve_Relative(void*, int)
	{
		return &Test_Globals;
	}

	void Copy_Bytes(int, void* Destination, std::size_t Size, const void* Source)
	{
		std::memcpy(Destination, Source, Size);
	}
}

namespace Redirection_Manager
{
	struct Manager_Structure
	{
		void* Caller;

		void Special_Call(void*, void*)
		{
			++Native_Updates;
		}
	};
}

#include "../SegaGmodX64/Priority.hpp"
#include "../SegaGmodX64/Post_Data_Update.hpp"
#include "../SegaGmodX64/Update_Animation.hpp"

namespace
{
	int Checks;
	int Failures;

	void Check(bool Condition, const char* Name)
	{
		++Checks;
		if (!Condition)
		{
			++Failures;
			std::printf("FAIL %s\n", Name);
		}
	}

	void Animate(void*) {}

	struct Fixture
	{
		alignas(16) std::array<unsigned char, 14784> Player{};
		alignas(16) std::array<unsigned char, 64> Table{};
		alignas(16) std::array<unsigned char, 72> Modifications{};
		alignas(16) std::array<unsigned char, 336> Animation{};

		template <typename T>
		void Set(std::size_t Offset, T Value)
		{
			std::memcpy(Player.data() + Offset, &Value, sizeof(Value));
		}

		Fixture()
		{
			Set(136, 1);
			Set(256, 16385);
			Set(508, static_cast<signed char>(2));
			Set(336, 100.f);
			Set(5688, Table.data());
			Set(13768, Animation.data());
			auto Pointer = Modifications.data();
			std::memcpy(Table.data() + 16, &Pointer, sizeof(Pointer));
		}

		void Network(double Time, double Previous_Time, float X, float Previous_X)
		{
			Set(176, Time);
			Set(184, Previous_Time);
			Set(1080, X);
			Set(820, Previous_X);
			Redirected_Post_Data_Update(Player.data() + 16, nullptr);
		}

		void Capture()
		{
			Redirected_Update_Animation(Player.data());
		}
	};
}

int main()
{
	Global_Variables_Structure Globals{};
	Globals.Time = 1.;
	Globals.Tick_Number = 100;
	Globals.Frame_Time = 0.002f;
	Globals.Interval_Per_Tick = 0.01;
	Test_Globals = &Globals;
	Update_Animation_Manager.Caller = reinterpret_cast<void*>(Animate);
	Player_Data_Structure& Data = Players_Data[1];
	Fixture First;

	First.Network(1., 0.9, 0.f, 0.f);
	Check(Data.Entity == First.Player.data() && Data.Handle == 16385, "network subobject maps to full player identity");
	Check(Data.Pending_Record && !Data.Has_Record && Data.Network_Tick == 100, "first update schedules capture");
	First.Capture();
	Check(Data.Has_Record && !Data.Pending_Record && Data.Motion_History.Count == 1, "animation commits network snapshot");
	Check(Data.Motion_History.Records[0].Time == 1. && Data.Motion_History.Records[0].Velocity[0] == 100.f, "snapshot uses simulation time and velocity");
	Check(Globals.Frame_Time == 0.002f, "animation restores frame time");

	Globals.Tick_Number = 101;
	First.Network(1., 1., 0.f, 0.f);
	First.Capture();
	Check(Data.Network_Tick == 100 && Data.Motion_History.Count == 1, "duplicate packet does not refresh snapshot age");

	Globals.Tick_Number = 110;
	First.Network(1.1, 1., 10.f, 0.f);
	Extrapolating_Player = true;
	First.Capture();
	Check(Data.Pending_Record && Data.Motion_History.Count == 1, "extrapolated animation cannot consume pending network snapshot");
	Extrapolating_Player = false;
	First.Capture();
	Check(!Data.Pending_Record && Data.Motion_History.Count == 2, "network capture resumes after extrapolation");

	Globals.Tick_Number = 111;
	First.Network(1.1, 1.1, 100.f, 10.f);
	Check(!Data.Has_Record && Data.Pending_Record && Data.Teleported, "same-time origin correction invalidates old snapshot");
	First.Capture();
	Check(Data.Motion_History.Count == 1 && Data.Motion_History.Records[0].Origin[0] == 100.f, "same-time origin correction becomes new baseline");
	Globals.Tick_Number = 112;
	First.Network(1.12, 1.1, 100.f, 100.f);
	First.Capture();
	Check(!Data.Teleported && Data.Has_Record, "stationary fresh sample clears teleport flag");

	First.Set(215, static_cast<signed char>(1));
	First.Network(1.2, 1.12, 100.f, 100.f);
	Check(!Data.Has_Record && !Data.Pending_Record && Data.Motion_History.Count == 0, "death clears snapshot and history");
	First.Set(215, static_cast<signed char>(0));
	First.Network(1.2, 1.2, 100.f, 100.f);
	First.Capture();
	Check(Data.Has_Record && Data.Motion_History.Count == 1, "respawn requires fresh snapshot");

	First.Set(514, static_cast<signed char>(1));
	First.Network(1.3, 1.2, 100.f, 100.f);
	Check(!Data.Has_Record && !Data.Pending_Record, "dormancy clears snapshot");
	First.Set(514, static_cast<signed char>(0));
	First.Network(1.3, 1.3, 100.f, 100.f);
	First.Capture();
	Check(Data.Has_Record && Data.Motion_History.Count == 1, "dormancy exit creates fresh baseline");

	First.Set(256, 32769);
	First.Network(1.4, 1.3, 110.f, 100.f);
	Check(Data.Handle == 32769 && !Data.Has_Record, "entity serial change invalidates snapshot");
	First.Capture();
	Check(Data.Motion_History.Count == 1, "entity serial change resets motion history");

	Fixture Second;
	Second.Set(256, 32769);
	Second.Network(1.5, 1.4, 120.f, 110.f);
	Check(Data.Entity == Second.Player.data() && !Data.Has_Record, "entity pointer replacement invalidates snapshot");
	Second.Capture();
	Check(Data.Motion_History.Count == 1, "entity pointer replacement creates new baseline");

	Second.Network(0.5, 1.5, 0.f, 0.f);
	Check(!Data.Has_Record && Data.Pending_Record, "simulation rewind invalidates snapshot");
	Second.Capture();
	Check(Data.Motion_History.Count == 1 && Data.Motion_History.Records[0].Time == 0.5, "simulation rewind creates new epoch");

	Globals.Tick_Number = 10;
	Second.Network(0.6, 0.5, 10.f, 0.f);
	Check(!Data.Has_Record && Data.Network_Tick == 10, "receive clock rewind invalidates snapshot");
	Second.Capture();
	Check(Data.Motion_History.Count == 1, "receive clock rewind resets motion history");

	Second.Set(336, std::numeric_limits<float>::quiet_NaN());
	Second.Network(0.7, 0.6, 10.f, 10.f);
	Second.Capture();
	Check(!Data.Has_Record && !Data.Pending_Record && Data.Motion_History.Count == 0, "invalid movement data cannot become a usable snapshot");
	Second.Set(336, 100.f);
	Second.Network(std::numeric_limits<double>::infinity(), 0.7, 10.f, 10.f);
	Check(!Data.Has_Record && !Data.Pending_Record, "nonfinite simulation time rejects network update");

	const int Previous_Updates = Native_Updates;
	Second.Set(136, 129);
	Second.Network(0.8, 0.7, 10.f, 10.f);
	Second.Capture();
	Second.Set(136, 0);
	Second.Network(0.8, 0.7, 10.f, 10.f);
	Second.Capture();
	Check(Native_Updates == Previous_Updates + 2, "invalid player indices still forward native network callback");

	Data.Priority = 7;
	Data.Reset_Record();
	Check(Data.Priority == 7 && Data.Entity == nullptr && Data.Motion_History.Count == 0, "record reset preserves priority");
	std::printf("CHECKS %d FAILURES %d\n", Checks, Failures);
	return Failures != 0;
}
