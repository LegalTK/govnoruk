float Normalize_Angle(float Angle)
{
	if (!__builtin_isfinite(Angle))
	{
		return 0.f;
	}

	Angle = __builtin_fmodf(Angle, 360.f);

	if (Angle >= 180.f)
	{
		Angle -= 360.f;
	}
	else if (Angle < -180.f)
	{
		Angle += 360.f;
	}

	return Angle == -0.f ? 0.f : Angle;
}

void Bruteforce_Reset_Memory_Tolerance()
{
	__int32 Player_Number = 0;

	Traverse_Players_Data_Label:
	{
		Player_Data_Structure* Player_Data = &Players_Data[Player_Number];

		if (Player_Data->Memory_Tolerance != 0)
		{
			Player_Data->Memory_Tolerance = Interface_Bruteforce_Memory_Tolerance.Get_Integer();
		}

		Player_Number += 1;

		if (Player_Number != sizeof(Players_Data) / sizeof(Player_Data_Structure))
		{
			goto Traverse_Players_Data_Label;
		}
	}
}

void Bruteforce_Reset_Tolerance()
{
	__int32 Player_Number = 0;

	Traverse_Players_Data_Label:
	{
		Player_Data_Structure* Player_Data = &Players_Data[Player_Number];

		if (Player_Data->Memory_Tolerance == 0)
		{
			Player_Data->Tolerance = Interface_Bruteforce_Tolerance.Get_Integer();
		}

		Player_Number += 1;

		if (Player_Number != sizeof(Players_Data) / sizeof(Player_Data_Structure))
		{
			goto Traverse_Players_Data_Label;
		}
	}
}

void Bruteforce_Reset()
{
	__int32 Player_Number = 0;

	Traverse_Players_Data_Label:
	{
		Player_Data_Structure* Player_Data = &Players_Data[Player_Number];

		Player_Data->Memory_Tolerance = 0;

		Player_Data->Tolerance = Interface_Bruteforce_Tolerance.Get_Integer();

		Player_Data->Shots_Fired = 0;

		Player_Data->Switch_X = 0;

		Player_Number += 1;

		if (Player_Number != sizeof(Players_Data) / sizeof(Player_Data_Structure))
		{
			goto Traverse_Players_Data_Label;
		}
	}
}

__int8 Bruteforce_Angles_Count;

float* Bruteforce_Angles;

float Bruteforce_Get_Angle(Player_Data_Structure* Player_Data)
{
	if (Bruteforce_Angles_Count <= 0 || Bruteforce_Angles == nullptr)
	{
		return 0.f;
	}

	__int32 Angle_Number = Player_Data->Shots_Fired;

	if (Angle_Number < 0 || Angle_Number >= Bruteforce_Angles_Count)
	{
		Angle_Number = ((Angle_Number % Bruteforce_Angles_Count) + Bruteforce_Angles_Count) % Bruteforce_Angles_Count;
		Player_Data->Shots_Fired = Angle_Number;
	}

	return Normalize_Angle(Bruteforce_Angles[Angle_Number]);
}

void Bruteforce_Set_Angles(Interface_Structure* Interface)
{
	Bruteforce_Angles_Count = 1;

	Bruteforce_Angles = (float*)__builtin_realloc(Bruteforce_Angles, Bruteforce_Angles_Count * sizeof(Bruteforce_Angles));

	Interface = (Interface_Structure*)((unsigned __int64)Interface - 48);

	Bruteforce_Angles[Bruteforce_Angles_Count - 1] = Normalize_Angle(atof(Interface->String));

	char* String = __builtin_strchr(Interface->String, ',');

	Set_Bruteforce_Angles_Label:
	{
		if (String != nullptr)
		{
			Bruteforce_Angles_Count += 1;

			Bruteforce_Angles = (float*)__builtin_realloc(Bruteforce_Angles, Bruteforce_Angles_Count * sizeof(Bruteforce_Angles));

			String = (char*)((unsigned __int64)String + 1);

			Bruteforce_Angles[Bruteforce_Angles_Count - 1] = Normalize_Angle(atof(String));

			String = __builtin_strchr(String, ',');

			goto Set_Bruteforce_Angles_Label;
		}
	}

	Bruteforce_Reset();
}