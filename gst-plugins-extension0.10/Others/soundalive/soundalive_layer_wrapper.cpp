//////////////////////////////////////
// SoundAlive_play interface declaration //
//////////////////////////////////////
class Samsung_SoundAlive_play_Interface
{
    public:

        // SoundAlive initialization
//        virtual int Init(void) = 0;
        // Shall be called only at the player first launch.

        // SoundAlive setting sampling rate
        virtual int TrackStart(int SamplingRate) = 0;
        // Should be called In case of new track start, seek, FF/RW, pause/resume instead of SoundAlive_Init()
        // SamplingRate - samplerate of current track
        // Default SamplingRate is 44100
        // Supported SamplingRates : 8000,11025,12000,16000,22050,24000,32000,44100,48000

        // Processing
        virtual int Exe(short (*OutBuf)[2], const short (*InBuf)[2], int n, int volume) = 0;
        // OutBuf, InBuf - 16-bit interleaved PCM {l/r/l/r/l/r/...}        // n - number of l/r pairs
        // Note: Each l/r pair occupies 4 bytes, so n=buflen>>2; where buflen is InBuf buffer length in bytes
        // volume - volume slider position (0..15)

        // Output configuration
//        virtual int OutDevConfig(short OutDev) = 0;
        // OutDev -  SA_EAR=0 : earphone
        // OutDev -  SA_SPK=1 : speaker
        virtual int Set_EarPhone_Output(void) = 0;
        virtual int Set_Speaker_Output(void) = 0;

        // Preset configuration
        virtual int Set_Preset(int Preset) = 0;
        virtual int Set_AutoPreset(int Preset) = 0;

        // Userset configuration
        virtual int Set_User_EQ(int *slider_EQ) = 0;
        // Eqlev - pointer to int (7 bands)    ( ex. int Eqlev[7]={+3,+2,+1,0,-1,-2,-3}; )

//        virtual int ExtParConfig(int m3Dlevel, int BElevel, int CHlevel, int CHRoomSize, int Clalevel) = 0;
        //  EQParConfig, ExtParConfig parameters corresponds to GUI slider range
        //  EQ :-12~12, 3D/BE/CHlevel/CHsize/Clarity : 0~3
        virtual int Set_User_3D(int slider_3D) = 0;
        virtual int Set_User_BE(int slider_BE) = 0;
        virtual int Set_User_CHlevel(int slider_CHlevel) = 0;
        virtual int Set_User_CHroomsize(int slider_CHroomsize) = 0;
        virtual int Set_User_Cla(int slider_Cla) = 0;

        // Navigation control
        virtual int Navigation_Button_Pressed(void) = 0;
//        virtual int Pause_Button_Pressed(void) = 0;
//        virtual int Restart_Button_Pressed(void) = 0;
//        virtual int Seek_Button_Pressed(void) = 0;

        virtual char* Get_Version(void) = 0;

        // Parameters getting functions
        virtual int Get_EQ_BandNum(void) = 0;
//        virtual int Get_EQ_Range(void);
        virtual int Get_EQ_BandFc(int band_num) = 0;
        virtual int Get_EQ_BandWidth(int band_num) = 0;


        virtual      ~Samsung_SoundAlive_play_Interface(void){};

};

class Samsung_SoundAlive_play_Factory
{
    public:
        static Samsung_SoundAlive_play_Interface * Create(void);
        static void Destroy(Samsung_SoundAlive_play_Interface * );
};

struct _SoundAlive_Handle
{
	Samsung_SoundAlive_play_Interface* A;
};
typedef struct _SoundAlive_Handle SA_Handle;

extern "C" SA_Handle *SoundAlive_Init(void)
{
	SA_Handle *sa_handle = new SA_Handle;

	/* create instance */
	sa_handle->A = Samsung_SoundAlive_play_Factory::Create();

	/* Initialization */
	//sa_handle->A->Init();

    return sa_handle;
}

extern "C" int SoundAlive_Set_SamplingRate(SA_Handle *sa_handle, int SamplingRate)
{
	return sa_handle->A->TrackStart(SamplingRate);
}

extern "C" int SoundAlive_Exe(SA_Handle *sa_handle, short (*outbuf)[2], const short (*inbuf)[2], int n, int vol)
{
	return sa_handle->A->Exe(outbuf, inbuf, n, vol);
}

extern "C" int SoundAlive_Set_OutDev(SA_Handle *sa_handle, short outdev)
{
    /* OutDev -  SA_EAR=0 : earphone
     *           SA_SPK=1 : speaker  */
	if (outdev)
		return sa_handle->A->Set_Speaker_Output();
	else
		return sa_handle->A->Set_EarPhone_Output();
}

extern "C" int SoundAlive_Set_Preset(SA_Handle *sa_handle, int preset)
{
	return sa_handle->A->Set_Preset(preset);
}

extern "C" int SoundAlive_Set_AutoPreset(SA_Handle *sa_handle, int preset)
{
	return sa_handle->A->Set_AutoPreset(preset);
}

extern "C" int SoundAlive_Set_User_EQ(SA_Handle *sa_handle, int *eqlev_list)
{
	return sa_handle->A->Set_User_EQ(eqlev_list);
}

extern "C" int SoundAlive_Set_User_3D(SA_Handle *sa_handle, int m3d_level)
{
	return sa_handle->A->Set_User_3D(m3d_level);
}

extern "C" int SoundAlive_Set_User_BE(SA_Handle *sa_handle, int be_level)
{
	return sa_handle->A->Set_User_BE(be_level);
}

extern "C" int SoundAlive_Set_User_CHlevel(SA_Handle *sa_handle, int ch_level)
{
	return sa_handle->A->Set_User_CHlevel(ch_level);
}

extern "C" int SoundAlive_Set_User_CHroomsize(SA_Handle *sa_handle, int chroomsize)
{
	return sa_handle->A->Set_User_CHroomsize(chroomsize);
}

extern "C" int SoundAlive_Set_User_Cla(SA_Handle *sa_handle, int cla_level)
{
	return sa_handle->A->Set_User_Cla(cla_level);
}

extern "C" int SoundAlive_Get_EQ_BandNum(SA_Handle *sa_handle)
{
	return sa_handle->A->Get_EQ_BandNum();
}

extern "C" int SoundAlive_Get_EQ_BandFc(SA_Handle *sa_handle, int band_num)
{
	return sa_handle->A->Get_EQ_BandFc(band_num);
}

extern "C" int SoundAlive_Get_EQ_BandWidth(SA_Handle *sa_handle, int band_num)
{
	return sa_handle->A->Get_EQ_BandWidth(band_num);
}

extern "C" int SoundAlive_Navigation_Button_Pressed(SA_Handle *sa_handle)
{
	return sa_handle->A->Navigation_Button_Pressed();
}

extern "C" char* SoundAlive_Get_Version(SA_Handle *sa_handle)
{
	return sa_handle->A->Get_Version();
}

extern "C" int SoundAlive_Reset(SA_Handle *sa_handle)
{
    /* destroy instance */
	Samsung_SoundAlive_play_Factory::Destroy(sa_handle->A);
    delete sa_handle;
    return 0;
}
