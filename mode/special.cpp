// There is evidence that they originally planned on having much larger special stages, probably 128x128. For one, many
// of the address calculations for accessing stage layout data use 0xFF0000 as the base address, even though the stage
// data is only loaded starting at 0xFF1000. It clears the area from 0xFF0000 to 0xFF1000 before loading the stage data.
// Also, the stage layouts are only 64x64, but the layout data uses 64 128-tile rows. If they did use 128x128 tiles,
// it would neatly fit in the area from 0xFF0000 to 0xFF4000 (where the tile info resides).

void GM_Special()
{
	PlaySound_Special(SFX_EnterSS);
	PaletteWhiteOut();
	DISABLE_INTERRUPTS();

	VDP_RegWrite(0x0B, 3);    // line scroll mode
	VDP_RegWrite(0x00, 4);    // 8-colour mode
	VDP_RegWrite(0x10, 0x11); // 128-cell hscroll size
	v_hbla_hreg = 0x8A00 + 175;
	VDP_Control(v_vdp_buffer1 & 0xFFBF);

	ClearScreen();
	ENABLE_INTERRUPTS();
	FillVRAM(0, 0x6FFF, 0x5000);
	VDP_WaitForDMA();

	VDP_RegWrite(0x0F, 2); // increment to 2 bytes
	SS_BGLoad()
	QuickPLC(PLC_SpecialStage);

	Clear_Objects();
	Clear_F700_F800()
	Clear_FE60_FF00();
	Clear_NGFX_Buffer();
	f_wtr_state = false;
	f_restart = 0;
	PalLoad1(Palette_Special);
	SS_Load();
	v_screenposx = 0;
	v_screenposy = 0;
	v_player->id = ID_SonicSpecial;
	PalCycle_SS();
	v_ssangle = 0;
	v_ssrotate = SS_InitialRotate;
	PlaySound(BGM_SS);
	v_btnpushtime1 = 0;
	v_btnpushtime2 = DemoDataPtr[6][1] - 1;
	v_rings = 0;
	v_lifecount = 0;
	v_debuguse = 0;
	v_demolength = 30 * OneSecond;

	if(f_debugcheat && v_jpadhold1 & Buttons_A)
		f_debugmode = true;

	VDP_Control(v_vdp_buffer1 | 0x40);
	PaletteWhiteIn();

	// ===========================================================================
	// Main Special Stage loop
	// ===========================================================================

	do
	{
		PauseGame();
		WaitForVBlank(VBlank_Special);
		MoveSonicInDemo();
		v_jpadhold2 = v_jpadhold1;
		ExecuteObjects();
		SS_ShowLayout(BuildSprites());
		SS_BGAnimate();

		if(f_demo && v_demolength == 0)
		{
			v_gamemode = GameMode_Sega;
			return;
		}
	} while(v_gamemode == GameMode_Special);

	if(f_demo)
	{
		if(v_gamemode == GameMode_Level)
			v_gamemode = GameMode_Sega;

		return;
	}

	v_gamemode = GameMode_Level;

	if(v_zone >= Zone_SBZ)
	{
		v_zone = 0;
		v_act = 0;
	}

	v_demolength = OneSecond;
	v_pfade_start = 63;
	v_palchgspeed = 0;

	do
	{
		WaitForVBlank(VBlank_SSFade);
		MoveSonicInDemo();
		v_jpadhold2 = v_jpadhold1;
		ExecuteObjects();
		SS_ShowLayout(BuildSprites());
		SS_BGAnimate();

		if(TimerZero(v_palchgspeed, 2)))
			WhiteOut_ToWhite();
	} while(v_demolength);

	DISABLE_INTERRUPTS();
	VDP_RegWrite(0x02, vram_fg >> 10); // set foreground nametable address
	VDP_RegWrite(0x04, vram_bg >> 13); // set background nametable address
	VDP_RegWrite(0x10, 1);             // 64-cell hscroll size
	ClearScreen();
	NemDec(Nem_TitleCard, 0xB000);
	Hud_Init();
	ENABLE_INTERRUPTS();
	PalLoad2(Palette_SSResult);
	NewPLC(PLC_Main);
	AddPLC(PLC_SSResult);
	f_scorecount = 1;
	f_endactbonus = 1;
	v_ringbonus = v_rings * 10;
	PlaySound_Special(BGM_GotThrough);

	Clear_Objects();
	v_objspace[23].id = ID_SSResult;

	do
	{
		PauseGame();
		WaitForVBlank(VBlank_SSResults);
		ExecuteObjects();
		BuildSprites();
		RunPLC();
	} while(!f_restart && v_plc_buffer != 0);

	PlaySound_Special(SFX_EnterSS);
	PaletteWhiteOut();
}

/*
SS VRAM nametable layout
0x5000: "all blocks" BG nametable
0x6000: transformation BG nametable
0x7000: transformation BG nametable
0x8000: "all fish" BG nametable
0x9000: transformation BG nametable
0xA000: transformation BG nametable
0xB000: "all birds" BG nametable
0xC000: "bubbles" BG nametable
0xE000: "clouds" BG nametable
*/

void SS_BGLoad()
{
	// Just using this as a temp buffer
	auto tilemapBuffer = (ushort*)v_sslayout;

	// Birds and fish
	EniDec(Eni_SSBg1, tilemapBuffer, 0x4051);

	// Outer loop: for each of the 7 graphics (bird, fish, and 5 transformation states)
	// i == 0 is the middle of the transformation, the background being entirely blocks, so it's copied to every "block"
	// of the BG.
	// i == 1~6 is the transparent graphics, which are copied to blocks of the BG in a checkerboard pattern.
	for(int i = 0; i < 7; i++)
	{
		// This flag business is used for making the checkerboard pattern. The birds and fish are on opposite squares,
		// so that's what the i > 3 is about (1, 2, 3 is birds, 4, 5, 6 is fish, or other way around, I dunno).
		bool checkerFlag = i > 3;

		// Four rows of eight columns each, and write them in a checkered pattern (except for i == 0 as mentioned above)
		for(int j = 0; j < 4; j++)
		{
			for(int k = 0; k < 8; k++)
			{
				checkerFlag = !checkerFlag;

				if(checkerFlag || i == 0)
					TilemapToVram(&tilemapBuffer[i * 0x80], 0x5000 + (i * 0x1000) + (j * 0x400) + (k * 0x10), 8, 8);
			}

			checkerFlag = !checkerFlag;
		}
	}

	// Clouds and Bubbles. The bubbles are at 0xC000, and the clouds at 0xE000. This code copies the bubble map twice
	// (so it can scroll vertically) to 0xC000-0xDFFF, and then copies the clouds map to 0xE000-0xEDFF. Since the clouds
	// only scroll horizontally, it doesn't matter that 0xEE00-0xFFFF contains non-map data.
	EniDec(Eni_SSBg2, tilemapBuffer, 0x4000);
	TilemapToVRAM(tilemapBuffer, 0xC000, 64, 32);
	TilemapToVRAM(tilemapBuffer, 0xD000, 64, 64);
}

const ubyte SS_BG_BubblesSections[] = { 10, 0x28, 0x18, 0x10, 0x28, 0x18, 0x10, 0x30, 0x18, 8, 0x10 };
const ubyte SS_BG_CloudsSections[] = { 7, 0x30, 0x30, 0x30, 0x28, 0x18, 0x18, 0x18 };
const byte SS_BG_BubblesOffsets[10][2] =
{
	{ 8,  2 },
	{ 4, -2 },
	{ 2,  3 },
	{ 8, -2 },
	{ 4,  2 },
	{ 2,  3 },
	{ 8, -3 },
	{ 4,  2 },
	{ 2,  3 },
	{ 2, -2 },
};

void SS_BGAnimate()
{
	if(v_ssbgstate == 0)
		v_bg1posy = v_bg1posy_dupx = 0;

	ushort* scrollPos;
	ubyte* scrollSections;

	// Precalculate the scroll values
	if(v_ssbgstate < 4)
	{
		// Bubbles
		if(v_ssbgstate == 3)
		{
			v_bg2posx++;
			v_bg1posy++;
			v_bg1posy_dupx = v_bg1posy;
		}

		// Wobble each section in a sine pattern
		for(int i = 0; i < 10; i++)
		{
			v_ssbubblescroll[i * 2] = (CalcSine(a3[i * 2 + 1]) * SS_BG_BubblesOffsets[i][0]) / 256;
			v_ssbubblescroll[i * 2 + 1] += SS_BG_BubblesOffsets[i][1];
		}

		scrollPos = v_ssbubblescroll;
		scrollSections = SS_BG_BubblesSections;
	}
	else
	{
		// Clouds
		if(v_ssbgstate == 6)
		{
			v_bg2posx--;

			// Scroll each section a decreasing amount
			for(int i = 0, scrollSpeed = 0x18000; i < 7; i++, scrollSpeed -= 0x2000)
				v_sscloudscroll[i] -= scrollSpeed;
		}

		scrollPos = v_sscloudscroll;
		scrollSections = SS_BG_CloudsSections;
	}

	// Write the scroll table
	auto scrollTable = v_hscrolltablebuffer;
	auto fgScroll = -v_bg2posx;
	auto scrollIdx = -v_bg1posy & 0xFF;

	for(int i = 0; i < scrollSections[0]; i++)
	{
		auto bgScroll = scrollPos[i * 2];

		for(int j = 0; j < scrollSections[i + 1]; j++)
			scrollTable[(scrollIdx++) & 0xFF] = (fgScroll << 16) | bgScroll;
	}
}

void SS_ShowLayout(ushort* spriteBuffer)
{
	SS_AniWallsRings();
	SS_UpdateAnims();

	// First: calculate all the tile screen positions based on the rotation angle
	int cosine;
	auto sine = CalcSine(v_ssangle & 0xFC, &cosine);
	auto xstep = sine * SS_TileSize;
	auto ystep = cosine * SS_TileSize;
	auto x = -(v_screenposx % SS_TileSize) - 180;
	auto y = -(v_screenposy % SS_TileSize) - 180;

	for(int i = 0; i < SS_PosBufferSize; i++)
	{
		// This is a typical 2D rotation
		auto xp = (-sine * y) + (cosine * x);
		auto yp = (cosine * y) + (sine * x);

		for(int j = 0; j < SS_PosBufferSize; j++)
		{
			v_ssposbuffer[i][j].x = xp >> 8;
			v_ssposbuffer[i][j].y = yp >> 8;
			xp += ystep;
			yp += xstep;
		}

		y += SS_TileSize;
	}

	// Next: actually draw the special stage tiles (as sprites!)
	for(int i = 0, row = v_screenposy / SS_TileSize; i < SS_PosBufferSize; i++, row++)
	{
		for(int j = 0, col = v_screenposx / SS_TileSize; j < SS_PosBufferSize; j++, col++)
		{
			auto tile = v_sslayout[row][col];
			auto xpos = v_ssposbuffer[i][j].x + 0x120;
			auto ypos = v_ssposbuffer[i][j].y + 0xF0;

			if(tile != SSObj_None && tile <= SSObj_MAX && ypos >= 0x70 && ypos < 0x170 && xpos >= 0x70 && xpos < 0x1D0)
			{
				auto tileInfo = v_sstileinfo[tile];
				auto mapping = tileInfo.map[tileInfo.frame];
				auto numPieces = *mapping++;
				EnqueueSpriteUnflipped(tileInfo.vram, mapping, numPieces, xpos, ypos, v_spritecount, spriteBuffer);
			}
		}
	}

	// Terminate sprite link list
	if(v_spritecount == MaxSprites)
		spriteBuffer[-5] = 0;
	else
		spriteBuffer[0] = 0;
}

const ushort SS_WaRiVramSet[] =
{
	0x0142, 0x6142, 0x0142, 0x0142, 0x0142, 0x0142, 0x0142, 0x6142,
	0x0142, 0x6142, 0x0142, 0x0142, 0x0142, 0x0142, 0x0142, 0x6142,
	0x2142, 0x0142, 0x2142, 0x2142, 0x2142, 0x2142, 0x2142, 0x0142,
	0x2142, 0x0142, 0x2142, 0x2142, 0x2142, 0x2142, 0x2142, 0x0142,
	0x4142, 0x2142, 0x4142, 0x4142, 0x4142, 0x4142, 0x4142, 0x2142,
	0x4142, 0x2142, 0x4142, 0x4142, 0x4142, 0x4142, 0x4142, 0x2142,
	0x6142, 0x4142, 0x6142, 0x6142, 0x6142, 0x6142, 0x6142, 0x4142,
	0x6142, 0x4142, 0x6142, 0x6142, 0x6142, 0x6142, 0x6142, 0x4142,
};

void SS_AniWallsRings()
{
	// Update wall tile frame numbers according to stage rotation
	auto frame = (v_ssangle / 4) & 0xF;

	for(int i = 0; i < SS_NumWallTileTypes; i++)
		v_sstileinfo[i].frame = frame;

	if(TimerNeg(v_ani1_time, 7))
		v_ani1_frame = (v_ani1_frame + 1) & 3;

	v_sstileinfo[58].frame = v_ani1_frame;

	if(TimerNeg(v_ani2_time, 7))
		v_ani2_frame = (v_ani2_frame + 1) & 1;

	v_sstileinfo[39].frame = v_ani2_frame;
	v_sstileinfo[41].frame = v_ani2_frame;
	v_sstileinfo[42].frame = v_ani2_frame;
	v_sstileinfo[44].frame = v_ani2_frame;
	v_sstileinfo[59].frame = v_ani2_frame;
	v_sstileinfo[60].frame = v_ani2_frame;
	v_sstileinfo[61].frame = v_ani2_frame;
	v_sstileinfo[62].frame = v_ani2_frame;
	v_sstileinfo[63].frame = v_ani2_frame;
	v_sstileinfo[64].frame = v_ani2_frame;

	if(TimerNeg(v_ani3_time, 4))
		v_ani3_frame = (v_ani3_frame + 1) & 3;

	v_sstileinfo[45].frame = v_ani3_frame;
	v_sstileinfo[46].frame = v_ani3_frame;
	v_sstileinfo[47].frame = v_ani3_frame;
	v_sstileinfo[48].frame = v_ani3_frame;

	if(TimerNeg(v_ani0_time, 7))
		v_ani0_frame = (v_ani0_frame + 1) & 7;

	for(int i = 0, tile = 2, vram = v_ani0_frame; i < 4; i++, vram += 8, tile += 9)
	{
		v_sstileinfo[tile].vram = SS_WaRiVramSet[vram];
		v_sstileinfo[tile + 1].vram = SS_WaRiVramSet[vram + 1];
		v_sstileinfo[tile + 2].vram = SS_WaRiVramSet[vram + 2];
		v_sstileinfo[tile + 3].vram = SS_WaRiVramSet[vram + 3];
		v_sstileinfo[tile + 4].vram = SS_WaRiVramSet[vram + 4];
		v_sstileinfo[tile + 5].vram = SS_WaRiVramSet[vram + 5];
		v_sstileinfo[tile + 6].vram = SS_WaRiVramSet[vram + 6];
		v_sstileinfo[tile + 7].vram = SS_WaRiVramSet[vram + 7];
	}
}

SS_Animation* SS_FindAnimSlot()
{
	for(int i = 0; i < SS_NumAnimations; i++)
	{
		if(v_ssanimations[i].id == 0)
			return &v_ssanimations[i];
	}

	return nullptr;
}

ubyte* SS_UpdateAnim(SS_Animation* anim, int timerReset, const ubyte* animData)
{
	if(TimerNeg(anim->timer, timerReset))
	{
		*anim->tile = animData[anim->frame++];

		if(*anim->tile == 0)
		{
			auto ret = anim->tile;
			memset(anim, 0, sizeof(SS_Animation));
			return ret;
		}
	}

	return nullptr;
}

void SS_UpdateAnims()
{
	for(int i = 0; i < SS_NumAnimations; i++)
	{
		switch(v_ssanimations[i]->id)
		{
			case SSAni_RingSparks: SS_UpdateAnim(&v_ssanimations[i], 5, SSAni_RingData);  break;
			case SSAni_1Up:        SS_UpdateAnim(&v_ssanimations[i], 5, SSAni_1UpData);   break;
			case SSAni_GlassBlock: SS_UpdateAnim(&v_ssanimations[i], 1, SSAni_GlassData); break;

			case SSAni_Bumper:
				if(auto tile = SS_UpdateAnim(&v_ssanimations[i], 7, SSAni_BumpData))
					*tile = SSObj_Bumper;
				break;

			case SSAni_Reverse:
				if(auto tile = SS_UpdateAnim(&v_ssanimations[i], 7, SSAni_BumpData))
					*tile = SSObj_R;
				break;

			case SSAni_EmeraldSparks:
				if(SS_UpdateAnim(&v_ssanimations[i], 5, SSAni_EmerData))
				{
					v_player->routine = Routine_ExitStage;
					PlaySound_Special(SFX_SSGoal);
				}
				break;

			case 0: break;
		}
	}
}

void SS_Load()
{
_retry:
	auto ssidx = v_lastspecial;
	IncWrap(v_lastspecial, NumEmeralds - 1);

	if(v_emeralds != NumEmeralds)
	{
		for(int i = v_emeralds - 1; i >= 0; i--)
		{
			if(v_emldlist[i] == 0)
				goto _retry;
		}
	}

	v_player->x = SS_StartLoc[ssidx].x;
	v_player->y = SS_StartLoc[ssidx].y;

	// Just using v_sstileinfo as a temp buffer
	ubyte* decompressedLayout = (ubyte*)v_sstileinfo;
	EniDec(SS_LayoutIndex[ssidx], decompressedLayout, 0);
	memset(v_sslayout, 0, 0x1000);

	// Have to expand 64-byte rows to 128-byte rows
	for(int i = 0; i < SS_LayoutUsedRows; i++)
		memcpy(v_sslayout[i + SS_LayoutRowStart], decompressedLayout[i * SS_LayoutUsedColumns], SS_LayoutUsedColumns);

	memcpy(v_sstileinfo, SS_MapIndex, SSObj_COUNT * sizeof(SSTileInfo));
	memset(v_ssanimations, 0, SS_NumAnimations * sizeof(SS_Animation));
}