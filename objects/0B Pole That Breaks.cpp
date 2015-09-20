// =====================================================================================================================
// Object 0B - pole in the water tunnels that breaks (LZ)
// These are the poles in the water tunnels that Sonic can grab onto and slide up and down for a bit before they break.
// =====================================================================================================================

enum : ubyte
{
	poleTimeW = 0x30,    // time between grabbing the pole & breaking
	poleGrabbedB = 0x32, // flag set when Sonic grabs the pole

	Routine_Init = 0,
	Routine_Action = 2,
	Routine_Display = 4,
}

enum : ushort
{
	GFX_Pole = 0x43DE;

	Top_OffsY = 24,
	Bottom_OffsY = 12,
	Catch_OffsX = 20,
}

const ubyte Map_Pole[][] =
{
	{2, 0xE0, 3, 0, 0, 0xFC, // normal
		0, 3, 0x10, 0, 0xFC},
	{4, 0xE0, 1, 0, 0, 0xFC, // broken
		0xF0, 5, 0, 4, 0xFC,
		0, 5, 0x10, 4, 0xFC,
		0x10, 1, 0x10, 0, 0xFC}
};

void Pole(Object* self)
{
	switch(self->routine)
	{
		case Routine_Init:
			self->routine = Routine_Action;
			self->map = Map_Pole;
			self->gfx = GFX_Pole;
			self->render = ObjRender::LayerNormal;
			self->actWid = 8;
			self->priority = 4;
			self->colType = 0xE1;
			VAR_W(self, poleTimeW) = self->subtype * 60;
			// fall through
		case Routine_Action:
			if(VAR_B(self, poleGrabbedB))
			{
				// Have we been hanging on too long?
				if(VAR_W(self, poleTimeW) != 0 && TimerZero(VAR_W(self, poleTimeW)))
				{
					self->frame = 1; // snap!
					ReleaseGrip();
				}
				else
				{
					// Sonic's controls are locked to up/down on the pole
					if(BTST(v_jpadhold1, Buttons::Up))
					{
						v_player->y--;
						auto top = self->y - Top_OffsY;

						if(v_player->y < top)
							v_player->y = top;
					}

					if(BTST(v_jpadhold1, Buttons::Down))
					{
						v_player->y++;
						auto bottom = self->y + Bottom_OffsY;

						if(v_player->y > bottom)
							v_player->y = bottom;
					}

					// Jump off
					if(v_jpadpress2 & Buttons::ABC)
						ReleaseGrip();
				}
			}
			else if(self->colProp != 0 && v_player->x > self->x + Catch_OffsX) // Should Sonic grab on?
			{
				// Yep
				self->colProp = 0;

				if(PlayerControllable())
				{
					v_player->velX = 0;
					v_player->velY = 0;
					v_player->x = self->x + Catch_OffsX;
					v_player->status = 0;
					v_player->anim = Anim::Hang;
					f_lockmulti = 1;
					f_wtunnelallow = true;
					VAR_B(self, poleGrabbedB) = 1;
				}
			}
			// fall through
		case Routine_Display:
			RememberState(self);
			break;
	}
}

void ReleaseGrip(Object* self)
{
	self->colType = 0;
	self->routine = Routine_Display
	f_lockmulti = 0;
	f_wtunnelallow = false;
	VAR_B(self, poleGrabbedB) = 0;
}