Redirection_Manager::Manager_Structure Post_Data_Update_Manager;

void Redirected_Post_Data_Update(void* Entity, void* Unknown_Parameter)
{
	const __int32 Player_Number = *(__int32*)((unsigned __int64)Entity + 120);

	if (Player_Number > 0 && Player_Number < static_cast<__int32>(sizeof(Players_Data) / sizeof(Players_Data[0])))
	{
		void* Player = (void*)((unsigned __int64)Entity - 16);
		Player_Data_Structure* Player_Data = &Players_Data[Player_Number];
		Global_Variables_Structure* Global_Variables = Get_Global_Variables();
		const __int32 Handle = *(__int32*)((unsigned __int64)Player + 256);
		const double Simulation_Time = *(double*)((unsigned __int64)Entity + 160);
		const double Previous_Simulation_Time = *(double*)((unsigned __int64)Entity + 168);

		if (Player_Data->Entity != Player || Player_Data->Handle != Handle || Global_Variables->Tick_Number < Player_Data->Network_Tick ||
			(Player_Data->Has_Record && Simulation_Time < Player_Data->Motion_History.Records[Player_Data->Motion_History.Count - 1].Time))
			Player_Data->Reset_Record();

		Player_Data->Entity = Player;
		Player_Data->Handle = Handle;

		if (!Extrapolation::Is_Finite(Simulation_Time) || Simulation_Time < 0. ||
			*(__int8*)((unsigned __int64)Player + 215) != 0 || *(__int8*)((unsigned __int64)Player + 514) != 0)
		{
			Player_Data->Reset_Record();
		}
		else
		{
			float* Origin = (float*)((unsigned __int64)Entity + 1064);
			float* Previous_Origin = (float*)((unsigned __int64)Entity + 804);
			const bool Corrected_Record = Player_Data->Has_Record &&
				Simulation_Time == Player_Data->Motion_History.Records[Player_Data->Motion_History.Count - 1].Time &&
				__builtin_memcmp(Origin, Previous_Origin, sizeof(float[3])) != 0;

			if (Simulation_Time > Previous_Simulation_Time || Corrected_Record || (!Player_Data->Has_Record && !Player_Data->Pending_Record))
			{
				float Distance_Squared = 0.f;
				for (__int32 Axis = 0; Axis < 3; ++Axis)
				{
					const float Difference = Origin[Axis] - Previous_Origin[Axis];
					Distance_Squared += Difference * Difference;
				}

				if (Corrected_Record)
				{
					Player_Data->Motion_History.Reset();
					Player_Data->Has_Record = false;
				}

				Player_Data->Teleported = Distance_Squared > 4096.f;
				Player_Data->Network_Tick = Global_Variables->Tick_Number;
				Player_Data->Pending_Record = true;
			}
		}
	}

	Post_Data_Update_Manager.Special_Call(Entity, Unknown_Parameter);
}
