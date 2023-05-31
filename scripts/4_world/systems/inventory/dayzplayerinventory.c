//Post event containers
class DeferredEvent
{
	InventoryMode m_mode;
	bool ReserveInventory(HumanInventory inventory){return true;}
	void ClearInventoryReservation(HumanInventory inventory){}
}


class DeferredTakeToDst : DeferredEvent
{
	ref InventoryLocation m_src;
	ref InventoryLocation m_dst;
	
	void DeferredTakeToDst(InventoryMode mode, notnull InventoryLocation src, notnull InventoryLocation dst)
	{
		m_mode = mode;
		m_src = src;
		m_dst = dst;
	}
	
	override bool ReserveInventory(HumanInventory inventory)
	{
		if( !inventory.AddInventoryReservationEx(m_dst.GetItem(), m_dst, GameInventory.c_InventoryReservationTimeoutShortMS) )
		{
			return false;
		}
		return true;
	}
	
	override void ClearInventoryReservation(HumanInventory inventory)
	{
		inventory.ClearInventoryReservationEx(m_dst.GetItem(), m_dst);
	}
}

class DeferredSwapEntities : DeferredEvent
{
	EntityAI m_item1;
	EntityAI m_item2;
	ref InventoryLocation m_dst1;
	ref InventoryLocation m_dst2;
	
	void DeferredSwapEntities(InventoryMode mode, notnull EntityAI item1, notnull EntityAI item2, notnull InventoryLocation dst1, notnull InventoryLocation dst2)
	{
		m_mode = mode;
		m_item1 = item1;
		m_item2 = item2;
		m_dst1 = dst1;
		m_dst2 = dst2;
	}
	
	override bool ReserveInventory(HumanInventory inventory)
	{
		if( !inventory.AddInventoryReservationEx(m_item1, m_dst1, GameInventory.c_InventoryReservationTimeoutShortMS) )
		{
			return false;
		}
		if( !inventory.AddInventoryReservationEx(m_item2, m_dst2, GameInventory.c_InventoryReservationTimeoutShortMS) )
		{
			inventory.ClearInventoryReservationEx(m_item1, m_dst1);
			return false;
		}
		return true;
	}
	
	override void ClearInventoryReservation(HumanInventory inventory)
	{
		inventory.ClearInventoryReservationEx(m_item1, m_dst1);
		inventory.ClearInventoryReservationEx(m_item2, m_dst2);
	}
}

class DeferredForceSwapEntities : DeferredEvent
{
	EntityAI m_item1;
	EntityAI m_item2;
	ref InventoryLocation m_dst1;
	ref InventoryLocation m_dst2;
	
	void DeferredForceSwapEntities(InventoryMode mode, notnull EntityAI item1, notnull EntityAI item2, notnull InventoryLocation dst1, notnull InventoryLocation dst2)
	{
		m_mode = mode;
		m_item1 = item1;
		m_item2 = item2;
		m_dst1 = dst1;
		m_dst2 = dst2;
	}
	
	override bool ReserveInventory(HumanInventory inventory)
	{
		if( !inventory.AddInventoryReservationEx(m_item1, m_dst1, GameInventory.c_InventoryReservationTimeoutShortMS) )
		{
			return false;
		}
		if( !inventory.AddInventoryReservationEx(m_item2, m_dst2, GameInventory.c_InventoryReservationTimeoutShortMS) )
		{
			inventory.ClearInventoryReservationEx(m_item1, m_dst1);
			return false;
		}
		return true;
	}
	
	override void ClearInventoryReservation(HumanInventory inventory)
	{
		inventory.ClearInventoryReservationEx(m_item1, m_dst1);
		inventory.ClearInventoryReservationEx(m_item2, m_dst2);
	}
}

class DeferredHandEvent: DeferredEvent
{
	ref HandEventBase m_event;
	void DeferredHandEvent(InventoryMode mode, HandEventBase e)
	{
		m_mode = mode;
		m_event = e;		
	}
	
	override bool ReserveInventory(HumanInventory inventory)
	{
		return m_event.ReserveInventory();
	}
	
	override void ClearInventoryReservation(HumanInventory inventory)
	{
		m_event.ClearInventoryReservation();
	}
}


/**
 * @class		DayZPlayerInventory
 **/
class DayZPlayerInventory : HumanInventoryWithFSM
{
	ref DeferredEvent m_DeferredEvent = NULL;
	ref Timer m_DeferredWeaponTimer = new Timer;
	//protected ref HandEventBase m_PostedHandEvent = NULL; /// deferred hand event
	
	
	protected ref HandEventBase m_DeferredPostedHandEvent = NULL; /// deferred hand event
	ref WeaponEventBase m_DeferredWeaponEvent = NULL; /// deferred weapon event
	// states with animations
	protected ref HandAnimatedTakingFromAtt m_Taking;
	protected ref HandAnimatedMovingToAtt m_MovingTo;
	protected ref HandAnimatedSwapping m_Swapping;
	protected ref HandAnimatedForceSwapping m_FSwapping;
	protected ref HandAnimatedForceSwapping_Inst m_FSwappingInst;

	void DayZPlayerInventory () 
	{
	}

	DayZPlayer GetDayZPlayerOwner () { return DayZPlayer.Cast(GetInventoryOwner()); }

	bool IsAuthoritative()
	{
		DayZPlayer player;
		if (!Class.CastTo(player, GetInventoryOwner()))
		{
			return false;
		}

		return (player.GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_CLIENT && player.GetInstanceType() != DayZPlayerInstanceType.INSTANCETYPE_REMOTE);
	}

	bool IsOwner()
	{
		DayZPlayer player;
		if (!Class.CastTo(player, GetInventoryOwner()))
		{
			return false;
		}

		return (player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT || player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SERVER || player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_SINGLEPLAYER);
	}

	bool IsProxy()
	{
		DayZPlayer player;
		if (!Class.CastTo(player, GetInventoryOwner()))
		{
			return false;
		}

		return (player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_REMOTE || player.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_AI_REMOTE);
	}

	override void Init ()
	{
		hndDebugPrint("[hndfsm] Creating DayZPlayer Inventory FSM");
		
		CreateStableStates(); // stable states needs to be created first

		m_Taking = new HandAnimatedTakingFromAtt(GetManOwner(), null);
		m_MovingTo = new HandAnimatedMovingToAtt(GetManOwner(), null);
		m_Swapping = new HandAnimatedSwapping(GetManOwner(), null);
		m_FSwapping = new HandAnimatedForceSwapping(GetManOwner(), null);
		m_FSwappingInst = new HandAnimatedForceSwapping_Inst(GetManOwner(), null);

		// events
		HandEventBase _fin_ = new HandEventHumanCommandActionFinished;
		HandEventBase _abt_ = new HandEventHumanCommandActionAborted;
		HandEventBase __T__ = new HandEventTake;
		HandEventBase __M__ = new HandEventMoveTo;
		HandEventBase __W__ = new HandEventSwap;
		//HandEventBase __D__ = new HandEventDropping;
		HandEventBase __Xd_ = new HandEventDestroyed;
		HandEventBase __F__ = new HandEventForceSwap;

		// setup transitions
		m_FSM.AddTransition(new HandTransition( m_Empty   , __T__,  m_Taking, NULL, new HandSelectAnimationOfTakeToHandsEvent(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_Taking  , _fin_,  m_Empty, null, new HandGuardNot(new HandGuardHasItemInHands(GetManOwner()))));
		m_FSM.AddTransition(new HandTransition( m_Taking  , _fin_,  m_Equipped, null, null));
		m_FSM.AddTransition(new HandTransition( m_Taking  , __Xd_,  m_Empty, new HandActionDestroyed, new HandGuardHasDestroyedItemInHands(GetManOwner())));
			m_Taking.AddTransition(new HandTransition(  m_Taking.m_Hide, _abt_,   m_Empty));
			m_Taking.AddTransition(new HandTransition(  m_Taking.m_Show, _abt_,   m_Equipped));

		m_FSM.AddTransition(new HandTransition( m_Equipped, __M__,  m_MovingTo, NULL, new HandSelectAnimationOfMoveFromHandsEvent(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_MovingTo, __Xd_,  m_Empty, new HandActionDestroyed, new HandGuardHasDestroyedItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_MovingTo, _fin_,  m_Equipped, null, new HandGuardHasItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_MovingTo, _fin_,  m_Empty   , null, null));
			m_MovingTo.AddTransition(new HandTransition(  m_MovingTo.m_Hide, _abt_,   m_Equipped));
			m_MovingTo.AddTransition(new HandTransition(  m_MovingTo.m_Show, _abt_,   m_Empty));

		m_FSM.AddTransition(new HandTransition( m_Equipped, __W__,  m_Swapping, NULL, new HandSelectAnimationOfSwapInHandsEvent(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_Swapping, __Xd_,  m_Empty, new HandActionDestroyed, new HandGuardHasDestroyedItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_Swapping, _fin_,  m_Empty, null, new HandGuardNot(new HandGuardHasItemInHands(GetManOwner()))));
		m_FSM.AddTransition(new HandTransition( m_Swapping, _fin_,  m_Equipped, null, null));
		m_FSM.AddTransition(new HandTransition( m_Swapping, _abt_,  m_Equipped, null, null));
		
		m_FSM.AddTransition(new HandTransition( m_Equipped, __F__, m_FSwappingInst, NULL, new HandGuardAnd( new HandSelectAnimationOfForceSwapInHandsEvent(GetManOwner()), new HandGuardInstantForceSwap(GetManOwner()) ) ));
		m_FSM.AddTransition(new HandTransition(m_FSwappingInst, _fin_,  m_Equipped, null, new HandGuardHasItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition(m_FSwappingInst, _fin_,  m_Empty, null, null));
		m_FSM.AddTransition(new HandTransition(m_FSwappingInst, __Xd_,  m_Empty, new HandActionDestroyed, new HandGuardHasDestroyedItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition( m_FSwappingInst, _abt_,  m_Equipped, null, null));

		m_FSM.AddTransition(new HandTransition( m_Equipped, __F__, m_FSwapping, NULL, new HandSelectAnimationOfForceSwapInHandsEvent(GetManOwner())));
		m_FSM.AddTransition(new HandTransition(m_FSwapping, _fin_,  m_Equipped, null, new HandGuardHasItemInHands(GetManOwner())));
		m_FSM.AddTransition(new HandTransition(m_FSwapping, _fin_,  m_Empty, null, null));
		m_FSM.AddTransition(new HandTransition(m_FSwapping, __Xd_,  m_Empty, new HandActionDestroyed, new HandGuardHasDestroyedItemInHands(GetManOwner())));
			m_FSwapping.AddTransition(new HandTransition(  m_FSwapping.m_Start, _abt_,   m_Equipped));
			m_FSwapping.AddTransition(new HandTransition(  m_FSwapping.m_Hide, _abt_,   m_Empty));
			m_FSwapping.AddTransition(new HandTransition(  m_FSwapping.m_Show, _abt_,   m_Equipped));
		
		super.Init(); // initialize ordinary human fsm (no anims)
	}
	
	/**@fn	CancelHandEvent
	 * @brief	cancels any handevents that will be executed this frame
	 * @NOTE: this is used in situations where the player performs an action that renders the event invalid exactly on the frame it will be executed
	 **/
	void CancelHandEvent ()
	{
		m_DeferredPostedHandEvent = null;
		//m_postedHandEvent = null;
	}
	
	void CancelWeaponEvent ()
	{
		m_DeferredWeaponEvent = null;
		m_DeferredWeaponTimer.Stop();
	}

	void AbortWeaponEvent ()
	{
		HumanCommandWeapons hcw = GetDayZPlayerOwner().GetCommandModifier_Weapons();
			
		Weapon_Base weapon;
		Class.CastTo(weapon, GetEntityInHands());
			
		if (hcw && weapon && weapon.CanProcessWeaponEvents() && !weapon.IsIdle())
		{
			if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(weapon) + " Weapon event: ABORT! notifying running state=" + weapon.GetCurrentState()); }
			weapon.ProcessWeaponAbortEvent(new WeaponEventHumanCommandActionAborted(GetDayZPlayerOwner()));
		}
	}
	
	/**@fn	PostWeaponEvent
	 * @brief	deferred weapon's fsm handling of events
	 * @NOTE: "post" stores the event for later use when ::CommandHandler is being run
	 **/
	void PostWeaponEvent (WeaponEventBase e)
	{
		if (m_DeferredWeaponEvent == NULL)
		{
			m_DeferredWeaponEvent = e;
			if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(GetInventoryOwner()) + " Posted event m_DeferredWeaponEvent=" + m_DeferredWeaponEvent.DumpToString()); }
		}
		else
			Error("[wpnfsm] " + Object.GetDebugName(GetInventoryOwner()) + " warning - pending event already posted, curr_event=" + m_DeferredWeaponEvent.DumpToString() + " new_event=" + e.DumpToString());
	}
	
	void DeferredWeaponFailed()
	{
		Weapon_Base weapon;
		Class.CastTo(weapon, GetEntityInHands());
		
		string secondPart = " - ENTITY IN HANDS IS NOT A WEAPON: " + Object.GetDebugName(GetEntityInHands());
		
		string firstPart = "[wpnfsm] " + Object.GetDebugName(GetInventoryOwner()) + " failed to perform weaponevent " + m_DeferredWeaponEvent.DumpToString();
		if (weapon)
		{
			secondPart = " on " + Object.GetDebugName(GetEntityInHands()) + " which is in state " + weapon.GetCurrentState();
			secondPart += " with physical state: J: " + weapon.IsJammed() + " | ";
			for (int i = 0; i < weapon.GetMuzzleCount(); ++i)
			{
				secondPart += "Chamber_" + i + ": B(" + weapon.IsChamberFull(i) + ") F(" + weapon.IsChamberFiredOut(i) + ") E(" + weapon.IsChamberEmpty(i) + ") | ";
				secondPart += "Magazine_" + i + ": " + weapon.GetMagazine(i);
				if (i < weapon.GetMuzzleCount() - 1)
					secondPart += " | ";
			}
		}
		
		Error(firstPart + secondPart);
		CancelWeaponEvent();
	}

	void HandleWeaponEvents (float dt, out bool exitIronSights)
	{
		HumanCommandWeapons hcw = GetDayZPlayerOwner().GetCommandModifier_Weapons();

		Weapon_Base weapon;
		Class.CastTo(weapon, GetEntityInHands());
		
		if (hcw && weapon && weapon.CanProcessWeaponEvents())
		{
			weapon.GetCurrentState().OnUpdate(dt);

			if (LogManager.IsWeaponLogEnable()) { wpnDebugSpamALot("[wpnfsm] " + Object.GetDebugName(weapon) + " HCW: playing A=" + typename.EnumToString(WeaponActions, hcw.GetRunningAction()) + " AT=" + WeaponActionTypeToString(hcw.GetRunningAction(), hcw.GetRunningActionType()) + " fini=" + hcw.IsActionFinished()); }

			if (!weapon.IsIdle())
			{
				while (true)
				{
					int weaponEventId = hcw.IsEvent();
					if (weaponEventId == -1)
					{
						break;
					}

					if (weaponEventId == WeaponEvents.CHANGE_HIDE)
					{
						break;
					}

					WeaponEventBase anim_event = WeaponAnimEventFactory(weaponEventId, GetDayZPlayerOwner(), NULL);
					if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(weapon) + " HandleWeapons: event arrived " + typename.EnumToString(WeaponEvents, weaponEventId) + "(" + weaponEventId + ")  fsm_ev=" + anim_event.ToString()); }
					if (anim_event != NULL)
					{
						weapon.ProcessWeaponEvent(anim_event);
					}
				}

				if (hcw.IsActionFinished())
				{
					if (weapon.IsWaitingForActionFinish())
					{
						if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(weapon) + " Weapon event: finished! notifying waiting state=" + weapon.GetCurrentState()); }
						weapon.ProcessWeaponEvent(new WeaponEventHumanCommandActionFinished(GetDayZPlayerOwner()));
					}
					else
					{
						if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(weapon) + " Weapon event: ABORT! notifying running state=" + weapon.GetCurrentState()); }
						weapon.ProcessWeaponAbortEvent(new WeaponEventHumanCommandActionAborted(GetDayZPlayerOwner()));
					}
				}
			}

			if (m_DeferredWeaponEvent)
			{
				if (LogManager.IsWeaponLogEnable()) { wpnDebugPrint("[wpnfsm] " + Object.GetDebugName(weapon) + " Weapon event: deferred " + m_DeferredWeaponEvent.DumpToString()); }
				if (weapon.ProcessWeaponEvent(m_DeferredWeaponEvent))
				{
					exitIronSights = true;
					fsmDebugSpam("[wpnfsm] " + Object.GetDebugName(weapon) + " Weapon event: resetting deferred event" + m_DeferredWeaponEvent.DumpToString());
					m_DeferredWeaponEvent = NULL;
					m_DeferredWeaponTimer.Stop();
				}
				else if (!m_DeferredWeaponTimer.IsRunning())
				{
					m_DeferredWeaponTimer.Run(3, this, "DeferredWeaponFailed");
				}
			}
		}
	}
	
	void HandleInventory(float dt)
	{
		HumanCommandWeapons hcw = GetDayZPlayerOwner().GetCommandModifier_Weapons();

		EntityAI ih = GetEntityInHands();
		Weapon_Base weapon;
		Class.CastTo(weapon, ih);
		
		if (hcw)
		{
			m_FSM.GetCurrentState().OnUpdate(dt);

			#ifdef DEVELOPER
			hndDebugSpamALot("[hndfsm] HCW: playing A=" + typename.EnumToString(WeaponActions, hcw.GetRunningAction()) + " AT=" + WeaponActionTypeToString(hcw.GetRunningAction(), hcw.GetRunningActionType()) + " fini=" + hcw.IsActionFinished());
			#endif
			
			if ( !m_FSM.GetCurrentState().IsIdle() || !m_FSM.IsRunning() )
			{
				while (true)
				{
					int weaponEventId = hcw.IsEvent();
					if (weaponEventId == -1)
					{
						break;
					}

					HandEventBase anim_event = HandAnimEventFactory(weaponEventId, GetManOwner(), NULL);
					#ifdef DEVELOPER
					hndDebugPrint("[hndfsm] HandleInventory: event arrived " + typename.EnumToString(WeaponEvents, weaponEventId) + "(" + weaponEventId + ")  fsm_ev=" + anim_event.ToString());
					#endif
					if (anim_event != NULL)
					{
						SyncHandEventToRemote(anim_event);
						ProcessHandEvent(anim_event);
					}
				}

				if (hcw.IsActionFinished())
				{
					if (m_FSM.GetCurrentState().IsWaitingForActionFinish())
					{
						#ifdef DEVELOPER
						hndDebugPrint("[hndfsm] Hand-Weapon event: finished! notifying waiting state=" + m_FSM.GetCurrentState());
						#endif
						HandEventBase fin_event = new HandEventHumanCommandActionFinished(GetManOwner());
						SyncHandEventToRemote(fin_event);
						ProcessHandEvent(fin_event);
					}
					else
					{
						#ifdef DEVELOPER
						hndDebugPrint("[hndfsm] Hand-Weapon event: ABORT! notifying running state=" + m_FSM.GetCurrentState());
						#endif
						HandEventBase abt_event = new HandEventHumanCommandActionAborted(GetManOwner());
						SyncHandEventToRemote(abt_event);						
						ProcessHandAbortEvent(abt_event);
						//m_FSM.ProcessHandAbortEvent(new WeaponEventHumanCommandActionAborted(GetManOwner()));
					}
				}
			}
		}
	}


	///@{ juncture
	/**@fn			OnInventoryJunctureFromServer
	 * @brief		reaction to engine callback
	 *	originates in:
	 *				engine - DayZPlayer::OnSyncJuncture
	 *				script - PlayerBase.OnSyncJuncture
	 **/
	override bool OnInventoryJunctureFromServer (ParamsReadContext ctx)
	{
		int tmp = -1;
		ctx.Read(tmp);
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " store Juncture packet STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp());
		
		StoreJunctureData(ctx);

		return true;
	}
	
	///@{ juncture
	/**@fn			OnInventoryJunctureFromServer
	 * @brief		reaction to engine callback
	 *	originates in:
	 *				engine - DayZPlayer::OnSyncJuncture
	 *				script - PlayerBase.OnSyncJuncture
	 **/
	override bool OnInventoryJunctureRepairFromServer (ParamsReadContext ctx)
	{
/*		InventoryLocation il = new InventoryLocation;
		if (!il.ReadFromContext(ctx) )
			return false;
		
		InventoryLocation il_current = new InventoryLocation;
		
		EntityAI item = il.GetItem();
		item.GetInventory().GetCurrentInventoryLocation(il_current);
		
		if( !il_current.CompareLocationOnly(il))
		{
			LocationMoveEntity(il_current,il);
		}*/
		return true;
	}
	
	///@{ juncture
	/**@fn			OnInventoryJunctureFailureFromServer
	 * @brief		reaction to engine callback
	 *	originates in:
	 *				engine - DayZPlayer::OnSyncJuncture
	 *				script - PlayerBase.OnSyncJuncture
	 **/
	override void OnInventoryJunctureFailureFromServer(ParamsReadContext ctx)
	{
		/**
		 * Function and setup is still messy due to the switch statement and relation with reading.
		 * 
		 * It could be cleaner if we used classes to handle each inventory command type, but that comes at a performance cost and will also probably require making a fair amount of changes elsewhere.
		 * 
		 * The downsides with this system right now:
		 * 1. It makes it hard to track what is written/read from the serializer
		 * 2. It makes this file very very large
		 * 
		 * The new changes at least remove the massive switch block and allow for all inventory commands to respond back to the client if something goes wrong
		 * 
		 */

		if (GetGame().IsDedicatedServer())
		{
			return;
		}

		int udtIdentifier = -1;
		if (!ctx.Read(udtIdentifier) || udtIdentifier != INPUT_UDT_INVENTORY)
		{
			return;
		}

		InventoryLocation src = new InventoryLocation;
		InventoryLocation dst = new InventoryLocation;
		InventoryLocation temp = new InventoryLocation;

		InventoryCommandType type = -1;
		if (!ctx.Read(type))
		{
			return;
		}

		switch (type)
		{
			case InventoryCommandType.SYNC_MOVE:
			{
				src.ReadFromContext(ctx);
				dst.ReadFromContext(ctx);
				break;
			}
			case InventoryCommandType.HAND_EVENT:
			{
				HandEventBase e = HandEventBase.CreateHandEventFromContext(ctx);
				src = e.GetSrc();
				dst = e.GetDst();
				break;
			}
			case InventoryCommandType.SWAP:
			{
				src.ReadFromContext(ctx);
				temp.ReadFromContext(ctx);
				dst.ReadFromContext(ctx);
				temp.ReadFromContext(ctx);
				break;
			}
			case InventoryCommandType.FORCESWAP:
			{
				break;
			}
			case InventoryCommandType.DESTROY:
			{
				src.ReadFromContext(ctx);
				break;
			}
		}

		InventoryValidationReason reason;
		if (!ctx.Read(reason))
		{
			reason = InventoryValidationReason.UNKNOWN;
		}

		OnInventoryFailure(type, reason, src, dst);
	}

	override void OnInventoryFailure(InventoryCommandType type, InventoryValidationReason reason, InventoryLocation src, InventoryLocation dst)
	{
		if (reason == InventoryValidationReason.DROP_PREVENTED)
		{
			//! TODO(kumarjac): Notify player here
			return;
		}

	}
	
	/**@fn			OnHandleStoredJunctureData
	 * @brief		reaction to engine callback
	 *	originates in engine - DayZPlayerInventory::HandleStoredJunctureData
	 **/
	protected void OnHandleStoredJunctureData (ParamsReadContext ctx)
	{
		int tmp = -1;
		ctx.Read(tmp);
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " handle JunctureData packet STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp());
		
		//! Juncture is never called on remote, always pass false
		ProcessInputData(ctx, true, false);
	}

	/**@fn			StoreJunctureData
	 * @brief		stores received input user data for later handling
	/aaa
	 **/
	proto native void StoreJunctureData (ParamsReadContext ctx);
	///@} juncture


	///@{ input user data
	/**@fn			OnInputUserDataProcess
	 * @brief		reaction to engine callback
	 *	originates in:
	 *				engine - DayZPlayer::OnInputUserDataReceived
	 *				script - DayZPlayerImplement.OnInputUserDataReceived
	 **/
	override bool OnInputUserDataProcess (ParamsReadContext ctx)
	{
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " store InputUserData packet STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp());
			StoreInputUserData(ctx);
		return true;
	}

	/**@fn			OnHandleStoredInputUserData
	 * @brief		reaction to engine callback
	 *	originates in engine - DayZPlayerInventory::HandleStoredInputUserData
	 **/
	protected void OnHandleStoredInputUserData (ParamsReadContext ctx)
	{
		int tmp = -1;
		ctx.Read(tmp);
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " handle InputUserData packet STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp());
		
		//! TODO(kumarjac): Is this correct, maybe it is doubling the action on the client?
		ProcessInputData(ctx, false, false);
	}

	/**@fn			StoreInputUserData
	 * @brief		stores received input user data for later handling
	 **/
	proto native void StoreInputUserData(ParamsReadContext ctx);
	///@} input user data
	
	void OnInputUserDataForRemote(ParamsReadContext ctx)
	{
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " remote handling InputUserData packet from server");
		
		ProcessInputData(ctx, false, true);
	}

	override void OnServerInventoryCommand(ParamsReadContext ctx)
	{
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " DZPInventory command from server");
		
		ProcessInputData(ctx, true, true);
	}

	bool ValidateSyncMove(inout Serializer ctx, InventoryValidation validation)
	{
		InventoryCommandType type = InventoryCommandType.SYNC_MOVE;

		InventoryLocation src = new InventoryLocation;
		InventoryLocation dst = new InventoryLocation;

		src.ReadFromContext(ctx);
		dst.ReadFromContext(ctx);
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + "src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst), "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
		}
		#endif
		
		if (validation.m_IsRemote && (!src.GetItem() || !dst.GetItem()))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - item not in bubble", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif

			syncDebugPrint("[syncinv] HandleInputData remote input (cmd=SYNC_MOVE) dropped, item not in bubble! src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
			return true;
		}

		EnableMovableOverride(src.GetItem());
		if (!PlayerCheckRequestSrc(src, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequestSrc", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " failed src check with cmd=" + typename.EnumToString(InventoryCommandType, type) + " src=" + InventoryLocation.DumpToStringNullSafe(src));		
			RemoveMovableOverride(src.GetItem());
			return true;
		}
		
		//! It is wasteful for remotes to determine whether some action is cheated or not, it has already happened on the server
		if (!validation.m_IsRemote && !PlayerCheckRequestDst(src, dst, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckMoveToDstRequest", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif

			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " is cheating with cmd=" + typename.EnumToString(InventoryCommandType, type) + " src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
			RemoveMovableOverride(src.GetItem());
			return true;
		}
		RemoveMovableOverride(src.GetItem());
		
		syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " HandleInputData t=" + GetGame().GetTime() + "ms received cmd=" + typename.EnumToString(InventoryCommandType, type) + " src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));

		//! Check if this this is being executed on the server and not by a juncture or AI
		if (!validation.m_IsJuncture && GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			JunctureRequestResult result_mv = TryAcquireInventoryJunctureFromServer(GetDayZPlayerOwner(), src, dst);
			if (result_mv == JunctureRequestResult.JUNCTURE_NOT_REQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture not required", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				//! TODO(kumarjac): Is this correct, shouldn't we just continue with exection of this method?

				LocationSyncMoveEntity(src, dst);

				validation.m_Result = InventoryValidationResult.SUCCESS;
				return true;
			}
			else if (result_mv == JunctureRequestResult.JUNCTURE_ACQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture sended", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				if (!GameInventory.LocationCanMoveEntity(src, dst))
				{
					#ifdef DEVELOPER
					DumpInventoryDebug();
					if ( LogManager.IsInventoryMoveLogEnable() )
					{
						Debug.InventoryMoveLog("Failed - LocationCanMoveEntity - Juncture denied", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
					}
					#endif

					return true;
				}
				
				validation.m_Result = InventoryValidationResult.JUNCTURE;
				EnableMovableOverride(src.GetItem());
				return true;
			}
			else if (result_mv == JunctureRequestResult.JUNCTURE_DENIED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture denied", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				validation.m_Reason = InventoryValidationReason.JUNCTURE_DENIED;
				return true;
			}
			else
			{
				Error("[syncinv] HandleInputData: unexpected return code from AcquireInventoryJunctureFromServer");

				return true;
			}
		}

		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT)
		{
			ClearInventoryReservationEx(dst.GetItem(), dst);
		}
		
		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			CheckForRope(src, dst);
		}

		if (!GameInventory.LocationCanMoveEntitySyncCheck(src, dst))
		{
			#ifdef DEVELOPER
			DumpInventoryDebug();
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - LocationCanMoveEntity", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			Error("[desync] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " CANNOT move cmd=" + typename.EnumToString(InventoryCommandType, type) + " src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
			return true;
		}
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("Success - LocationSyncMoveEntity", "SYNC_MOVE" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
		}
		#endif

		LocationSyncMoveEntity(src, dst);

		validation.m_Result = InventoryValidationResult.SUCCESS;
		return true;
	}

	bool ValidateHandEvent(inout Serializer ctx, InventoryValidation validation)
	{
		InventoryCommandType type = InventoryCommandType.HAND_EVENT;

		HandEventBase e = HandEventBase.CreateHandEventFromContext(ctx);
		e.ClearInventoryReservation();

		EntityAI itemSrc = e.GetSrcEntity();
		EntityAI itemDst = e.GetSecondSrcEntity();

		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + e.m_Player.GetSimulationTimeStamp() + " event= " + e.ToString(), "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
		}
		#endif

		if (validation.m_IsRemote && !e.GetSrcEntity())
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequestSrc", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
			}
			#endif
			
			Error("[syncinv] HandleInputData remote input (cmd=HAND_EVENT) dropped, item not in bubble");
					
			//! TODO(kumarjac):	Are we handling this edge case elsewhere? If a player picks up an item that the remote can't see, how do we re-synchronize the event?
			//! 				Is this because we are hoping for the item to be "created" on the remote due to the automatically enlarged network bubble for the item 
			//!					always being enforced for players? What if that create message is recieved later? Does the hands on other clients become desynchronized?
			return true;
		}
		
		//! TODO(kumarjac):	Is this one correct to be 'RemoveMovableOverride' or are the other Validate methdos wrong with 'EnableMovableOverride'?
		if (itemSrc)
			RemoveMovableOverride(itemSrc);
		if (itemDst)
			RemoveMovableOverride(itemDst);

		if (!e.CheckRequestSrc())
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequestSrc", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
			}
			#endif
			
			if (!validation.m_IsRemote)
			{
				ctx = new ScriptInputUserData;
				InventoryInputUserData.SerializeHandEvent(ctx, e);
			}

			if (itemSrc)
				RemoveMovableOverride(itemSrc);
			if (itemDst)
				RemoveMovableOverride(itemDst);

			return true;
		}

		//! It is wasteful for remotes to determine whether some action is cheated or not, it has already happened on the server
		if (!validation.m_IsRemote && !e.CheckRequestEx(validation))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequest", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
			}
			#endif

			//! event 'CheckRequestEx' updates failure reason

			ctx = new ScriptInputUserData;
			InventoryInputUserData.SerializeHandEvent(ctx, e);
			
			//! TODO(kumarjac): Not a cheat, could be desync
			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " is cheating with cmd=" + typename.EnumToString(InventoryCommandType, type) + " event=" + e.DumpToString());
			if (itemSrc)
				RemoveMovableOverride(itemSrc);
			if (itemDst)
				RemoveMovableOverride(itemDst);
			return true;
		}
		
		if (!e.CanPerformEventEx(validation))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CanPerformEvent", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
			}
			#endif

			//! event 'CanPerformEventEx' updates failure reason

			ctx = new ScriptInputUserData;
			InventoryInputUserData.SerializeHandEvent(ctx, e);
			
			syncDebugPrint("[desync] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " CANNOT do cmd=HAND_EVENT e=" + e.DumpToString());
			if (itemSrc)
				RemoveMovableOverride(itemSrc);
			if (itemDst)
				RemoveMovableOverride(itemDst);
			return true;
		}
		if (itemSrc)
			RemoveMovableOverride(itemSrc);
		if (itemDst)
			RemoveMovableOverride(itemDst);

		//! Check if this this is being executed on the server and not by a juncture or AI
		if (!validation.m_IsJuncture && GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			JunctureRequestResult result_ev = e.AcquireInventoryJunctureFromServer(GetDayZPlayerOwner());
			if (result_ev == JunctureRequestResult.JUNCTURE_NOT_REQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture not required", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
				}
				#endif

				//! Continuing on with execution of rest of the function
			}
			else if (result_ev == JunctureRequestResult.JUNCTURE_ACQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture sended", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
				}
				#endif
				
				ctx = new ScriptInputUserData;
				InventoryInputUserData.SerializeHandEvent(ctx, e);
			
				validation.m_Result = InventoryValidationResult.JUNCTURE;
				
				if (itemSrc)
					EnableMovableOverride(itemSrc);
				if (itemDst)
					EnableMovableOverride(itemDst);
				return true;
			}
			else if (result_ev == JunctureRequestResult.JUNCTURE_DENIED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture denied", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
				}
				#endif

				validation.m_Reason = InventoryValidationReason.JUNCTURE_DENIED;
				return true;
			}
			else
			{
				Error("[syncinv] HandleInputData: unexpected return code from AcquireInventoryJunctureFromServer");
				
				return true;
			}
		}
		
		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			CheckForRope(e.GetSrc(), e.GetDst());
		}

		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("Success - ProcessHandEvent", "HAND_EVENT" , "n/a", "ProcessInputData", e.m_Player.ToString() );
		}
		#endif
		
		validation.m_Result = InventoryValidationResult.SUCCESS;
		if (!e.m_Player.GetHumanInventory().ProcessHandEvent(e))
		{
			//! TODO(kumarjac): We should probably set the result to failure like so
			//result = InventoryValidationResult.FAILURE;
		}

		return true;
	}

	bool ValidateSwap(inout Serializer ctx, InventoryValidation validation)
	{
		InventoryCommandType type = InventoryCommandType.SWAP;

		InventoryLocation src1 = new InventoryLocation;
		InventoryLocation src2 = new InventoryLocation;

		InventoryLocation dst1 = new InventoryLocation;
		InventoryLocation dst2 = new InventoryLocation;

		bool skippedSwap = false;
		
		src1.ReadFromContext(ctx);
		src2.ReadFromContext(ctx);
		dst1.ReadFromContext(ctx);
		dst2.ReadFromContext(ctx);
		ctx.Read(skippedSwap);
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " dst1=" + InventoryLocation.DumpToStringNullSafe(dst1) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2) + " dst2=" + InventoryLocation.DumpToStringNullSafe(dst2), "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
		}
		#endif

		if (validation.m_IsRemote && (!src1.GetItem() || !src2.GetItem()))
		{
			if (skippedSwap)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Remote - skipped", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif
				//syncDebugPrint("[syncinv] HandleInputData remote input (cmd=SWAP) dropped, swap is skipped");
			}
			else
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Failed - item1 or item2 not exist", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				Error("[syncinv] HandleInputData remote input (cmd=SWAP) dropped, item not in bubble");
			}

			return true;
		}
				
		EnableMovableOverride(src1.GetItem());
		EnableMovableOverride(src2.GetItem());
		
		if (!PlayerCheckRequestSrc(src1, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequestSrc1", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " failed src1 check with cmd=" + typename.EnumToString(InventoryCommandType, type) + " src1=" + InventoryLocation.DumpToStringNullSafe(src1));
			RemoveMovableOverride(src1.GetItem());
			RemoveMovableOverride(src2.GetItem());
			return true;
		}

		if (!PlayerCheckRequestSrc(src2, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckRequestSrc2", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " failed src2 check with cmd=" + typename.EnumToString(InventoryCommandType, type) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2));
			RemoveMovableOverride(src1.GetItem());
			RemoveMovableOverride(src2.GetItem());
			return true;
		}

		//! It is wasteful for remotes to determine whether some action is cheated or not, it has already happened on the server
		if (!validation.m_IsRemote && !PlayerCheckSwapItemsRequest(src1, src2, dst1, dst2, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CheckSwapItemsRequest", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			syncDebugPrint("[cheat] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " is cheating with cmd=" + typename.EnumToString(InventoryCommandType, type) + " src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2) +  " dst1=" + InventoryLocation.DumpToStringNullSafe(dst1) + " dst2=" + InventoryLocation.DumpToStringNullSafe(dst2));
			RemoveMovableOverride(src1.GetItem());
			RemoveMovableOverride(src2.GetItem());
			return true;
		}
				
		RemoveMovableOverride(src1.GetItem());
		RemoveMovableOverride(src2.GetItem());
		
		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT)
		{
			ClearInventoryReservationEx(dst1.GetItem(), dst1);
			ClearInventoryReservationEx(dst2.GetItem(), dst2);
		}				
		
		if (!GameInventory.CanForceSwapEntitiesEx(src1.GetItem(), dst1, src2.GetItem(), dst2))
		{
			#ifdef DEVELOPER
			DumpInventoryDebug();
				
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed - CanForceSwapEntitiesEx", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			Error("[desync] HandleInputData man=" + Object.GetDebugName(GetManOwner()) + " CANNOT swap cmd=" + typename.EnumToString(InventoryCommandType, type) + " src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " dst1=" + InventoryLocation.DumpToStringNullSafe(dst1) +" | src2=" + InventoryLocation.DumpToStringNullSafe(src2) + " dst2=" + InventoryLocation.DumpToStringNullSafe(dst2));
			return true;
		}
	
		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT)
		{
			AddInventoryReservationEx(dst1.GetItem(), dst1, GameInventory.c_InventoryReservationTimeoutShortMS);
			AddInventoryReservationEx(dst2.GetItem(), dst2, GameInventory.c_InventoryReservationTimeoutShortMS);
		}

		if (!(src1.IsValid() && src2.IsValid() && dst1.IsValid() && dst2.IsValid()))
		{
			Error("HandleInputData: cmd=" + typename.EnumToString(InventoryCommandType, type) + " invalid input(s): src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2) +  " dst1=" + InventoryLocation.DumpToStringNullSafe(dst1) + " dst2=" + InventoryLocation.DumpToStringNullSafe(dst2));	
			return true;
		}

		//! Check if this this is being executed on the server and not by a juncture or AI
		if (!validation.m_IsJuncture && GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			JunctureRequestResult result_sw = TryAcquireTwoInventoryJuncturesFromServer(GetDayZPlayerOwner(), src1, src2, dst1, dst2);
			if (result_sw == JunctureRequestResult.JUNCTURE_NOT_REQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture not required", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				//! Continuing on with execution of rest of the function
			}
			else if (result_sw == JunctureRequestResult.JUNCTURE_ACQUIRED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture sended", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				validation.m_Result = InventoryValidationResult.JUNCTURE;
				EnableMovableOverride(src1.GetItem());
				EnableMovableOverride(src2.GetItem());
				return true;
			}
			else if (result_sw == JunctureRequestResult.JUNCTURE_DENIED)
			{
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Juncture denied", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
				}
				#endif

				validation.m_Reason = InventoryValidationReason.JUNCTURE_DENIED;
				return true;
			}
			else
			{
				Error("[syncinv] HandleInputData: unexpected return code from TryAcquireTwoInventoryJuncturesFromServer"); return true;
				
				return true;
			}
		}
			
		if (GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_CLIENT )
		{
			ClearInventoryReservationEx(dst1.GetItem(),dst1);
			ClearInventoryReservationEx(dst2.GetItem(),dst2);
		}
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("Success - item swap", "SWAP" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );
		}
		#endif

		bool isNotSkipped = LocationSwap(src1, src2, dst1, dst2);
		
		ctx = new ScriptInputUserData();
		InventoryInputUserData.SerializeSwap(ctx, src1, src2, dst1, dst2, !isNotSkipped);

		validation.m_Result = InventoryValidationResult.SUCCESS;
		return true;
	}

	bool ValidateDestroy(inout Serializer ctx, InventoryValidation validation)
	{
		InventoryCommandType type = InventoryCommandType.DESTROY;

		if (validation.m_IsJuncture)
		{
			/** TODO(kumarjac):
			 *	Probably should be called through inventory juncture, 
			 *	we shouldn't allow the client to delete until the 
			 *	server says it is okay as there can be more reasons 
			 *	than "cheater" for it to be rejected such as desync
			 */
			
			return true;
		}

		InventoryLocation src = new InventoryLocation;
		src.ReadFromContext(ctx);
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " src=" + InventoryLocation.DumpToStringNullSafe(src), "DESTROY" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
		}
		#endif

		if (validation.m_IsRemote && !src.GetItem())
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed item not exist", "DESTROY" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
			}
			#endif

			Error("[syncinv] HandleInputData remote input (cmd=DESTROY) dropped, item not in bubble");
			return true;
		}
		
		if (!PlayerCheckRequestSrc(src, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed CheckRequestSrc", "DESTROY" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
			}
			#endif

			return true;
		}

		//! It is wasteful for remotes to determine whether some action is cheated or not, it has already happened on the server
		if (!validation.m_IsRemote && !PlayerCheckDropRequest(src, GameInventory.c_MaxItemDistanceRadius))
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("Failed CheckDropRequest", "DESTROY" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
			}
			#endif

			return true;
		}

		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("Success ObjectDelete", "DESTROY" , "n/a", "ProcessInputData", GetDayZPlayerOwner().ToString() );	
		}
		#endif

		GetGame().ObjectDelete(src.GetItem());

		validation.m_Result = InventoryValidationResult.SUCCESS;
		return true;
	}

	/**
	 * 
	 * @return false on malformed data, true on anything else, including cheats
	 */
	bool ProcessInputData(ParamsReadContext ctx, bool isJuncture, bool isRemote)
	{
		if (isJuncture && isRemote)
		{
			//! It should be impossible for juncture to be ran on remote
			return false;
		}

		int type = -1;
		if (!ctx.Read(type))
		{
			return false;
		}

		InventoryValidation validation();
		validation.m_IsJuncture = isJuncture;
		validation.m_IsRemote = isRemote;

		//! Serializer can be updated and re-written to when we may want to only correct the client instead of denying the inventory command
		Serializer serializer = ctx;
		
		switch (type)
		{
		case InventoryCommandType.USER_RESERVATION_CANCEL:
			if (!ValidateUserReservationCancel(serializer, validation))
			{
				return false;
			}
			break;
		case InventoryCommandType.SYNC_MOVE:
			if (!ValidateSyncMove(serializer, validation))
			{
				return false;
			}
			break;
		case InventoryCommandType.HAND_EVENT:
			if (!ValidateHandEvent(serializer, validation))
			{
				return false;
			}
			break;
		case InventoryCommandType.SWAP:
			if (!ValidateSwap(serializer, validation))
			{
				return false;
			}
			break;
		case InventoryCommandType.DESTROY:
			if (!ValidateDestroy(serializer, validation))
			{
				return false;
			}
			break;
		default:
			break;
		}

		bool canSendJuncture = !isJuncture && GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER;
		
		switch (validation.m_Result)
		{
		case InventoryValidationResult.FAILED:
			if (canSendJuncture) //! Only inform client about failure
			{
				//! General purpose serializer change from read to write here if the validation doesn't update it
				if (!serializer.CanWrite())
				{
					ScriptInputUserData writeableSerializer();
					writeableSerializer.CopyFrom(serializer);
					serializer = writeableSerializer;
				}

				serializer.Write(validation.m_Reason);

				GetDayZPlayerOwner().SendSyncJuncture(DayZPlayerSyncJunctures.SJ_INVENTORY_FAILURE, serializer);
			}
			break;
		case InventoryValidationResult.JUNCTURE:
			if (canSendJuncture) //! Only send juncture back to client
			{
				GetDayZPlayerOwner().SendSyncJuncture(DayZPlayerSyncJunctures.SJ_INVENTORY, serializer);
				StoreInputForRemotes(isJuncture, isRemote, serializer);
			}
			else
			{
				Error("InventoryValidationResult.JUNCTURE returned when not possible to send!");
			}
			break;
		case InventoryValidationResult.SUCCESS:
			StoreInputForRemotes(isJuncture, isRemote, serializer);
			break;
		}

		return true;
	}
	
	void RemoveMovableOverride(EntityAI item)
	{
		ItemBase itemIB = ItemBase.Cast(item);
		if (itemIB)
			itemIB.SetCanBeMovedOverride(false);
	}
	
	void EnableMovableOverride(EntityAI item)
	{
		ItemBase itemIB = ItemBase.Cast(item);
		if (itemIB)
			itemIB.SetCanBeMovedOverride(true);
	}
	
	// Hacky solution for dealing with fencekit rope related issues, could be fixed by introducing some indicator that this item behaves differently or sth..
	void CheckForRope(InventoryLocation src, InventoryLocation dst)
	{
		Rope rope = Rope.Cast(src.GetItem());
		if (rope)
			rope.SetTargetLocation(dst);
	}
	
	bool IsServerOrLocalPlayer()
	{
		return GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER || GetDayZPlayerOwner() == GetGame().GetPlayer());
	}
	
	bool StoreInputForRemotes (bool handling_juncture, bool remote, ParamsReadContext ctx)
	{
		if (!handling_juncture && !remote && GetDayZPlayerOwner().GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryMoveLogEnable() )
			{
				Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp(), "n/a" , "n/a", "StoreInputForRemotes", GetDayZPlayerOwner().ToString() );	
			}
			#endif
			GetDayZPlayerOwner().StoreInputForRemotes(ctx); // @NOTE: needs to be called _after_ the operation
			return true;
		}
		return false;
	}
	
	override bool TakeToDst (InventoryMode mode, notnull InventoryLocation src, notnull InventoryLocation dst)
	{
		if (!GetManOwner().IsAlive())
			return super.TakeToDst(mode,src,dst);
		
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst), "n/a" , "n/a", "TakeToDst", GetDayZPlayerOwner().ToString() );	
		}
		#endif
		
		switch ( mode )
		{
			case InventoryMode.SERVER:
				if (RedirectToHandEvent(mode, src, dst))
				{
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryMoveLogEnable() )
					{
						Debug.InventoryMoveLog("RedirectToHandEvent", "n/a" , "n/a", "TakeToDst", GetDayZPlayerOwner().ToString() );	
					}
					#endif
					return true;
				}
					

				if (GetDayZPlayerOwner().NeedInventoryJunctureFromServer(src.GetItem(), src.GetParent(), dst.GetParent()))
				{
					
					if (GetGame().AddInventoryJunctureEx(GetDayZPlayerOwner(), src.GetItem(), dst, true, GameInventory.c_InventoryReservationTimeoutMS))
					{
						syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " DZPI::Take2Dst(" + typename.EnumToString(InventoryMode, mode) + ") got juncture");
				
					}
					else
					{
						#ifdef DEVELOPER
						if ( LogManager.IsInventoryMoveLogEnable() )
						{
							Debug.InventoryMoveLog("Juncture failed", "n/a" , "n/a", "TakeToDst", GetDayZPlayerOwner().ToString() );	
						}
						#endif
						syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " DZPI::Take2Dst(" + typename.EnumToString(InventoryMode, mode) + ") got juncture");
						return false;
					}
						
				}

				ScriptInputUserData ctx = new ScriptInputUserData;
				InventoryInputUserData.SerializeMove(ctx, InventoryCommandType.SYNC_MOVE, src, dst);
	
				GetDayZPlayerOwner().SendSyncJuncture(DayZPlayerSyncJunctures.SJ_INVENTORY, ctx);
			
				syncDebugPrint("[syncinv] " + Object.GetDebugName(GetDayZPlayerOwner()) + " STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " store input for remote - DZPI::Take2Dst(" + typename.EnumToString(InventoryMode, mode) + " server sync move src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
				GetDayZPlayerOwner().StoreInputForRemotes(ctx); // @TODO: is this right place? maybe in HandleInputData(server=true, ...)
				#ifdef DEVELOPER
				if ( LogManager.IsInventoryMoveLogEnable() )
				{
					Debug.InventoryMoveLog("Success - store input for remote mode - " + typename.EnumToString(InventoryMode, mode) + " src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst), "n/a" , "n/a", "TakeToDst", GetDayZPlayerOwner().ToString() );	
				}
				#endif
				return true;
			
			case InventoryMode.LOCAL:
				LocationSyncMoveEntity(src, dst);
				return true;
		}
		if(!super.TakeToDst(mode,src,dst))
		{
			if (!m_DeferredEvent)
			{
				m_DeferredEvent = new DeferredTakeToDst(mode,src,dst);
				if( m_DeferredEvent.ReserveInventory(this) )
					return true;
			}
			
			m_DeferredEvent = null;
			return false;
		}
		return true;
	}
	
	void HandleTakeToDst( DeferredEvent deferred_event )
	{
		DeferredTakeToDst deferred_take_to_dst = DeferredTakeToDst.Cast(deferred_event);
		if( deferred_take_to_dst )
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryHFSMLogEnable() )
			{	
				Debug.InventoryHFSMLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp(), "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			
			deferred_take_to_dst.ClearInventoryReservation(this);
			inventoryDebugPrint("[inv] I::Take2Dst(" + typename.EnumToString(InventoryMode, deferred_take_to_dst.m_mode) + ") src=" + InventoryLocation.DumpToStringNullSafe(deferred_take_to_dst.m_src) + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_take_to_dst.m_dst));

			switch (deferred_take_to_dst.m_mode)
			{
				case InventoryMode.PREDICTIVE:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("PREDICTIVE ", "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );
					}
					#endif
				
					if (LocationCanMoveEntity(deferred_take_to_dst.m_src,deferred_take_to_dst.m_dst))
					{
						InventoryInputUserData.SendInputUserDataMove(InventoryCommandType.SYNC_MOVE, deferred_take_to_dst.m_src, deferred_take_to_dst.m_dst);
						LocationSyncMoveEntity(deferred_take_to_dst.m_src, deferred_take_to_dst.m_dst);
					}
					else
					{
						#ifdef DEVELOPER
						if ( LogManager.IsInventoryMoveLogEnable() )
						{
							Debug.InventoryMoveLog("Can not move entity (PREDICTIVE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item = " + deferred_take_to_dst.m_dst.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_take_to_dst.m_dst), "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );	
						}
						#endif
					}
					break;
				case InventoryMode.JUNCTURE:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("JUNCTURE ", "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );
					}
					#endif
				
					if (LocationCanMoveEntity(deferred_take_to_dst.m_src,deferred_take_to_dst.m_dst))
					{
						DayZPlayer player = GetGame().GetPlayer();
						player.GetHumanInventory().AddInventoryReservationEx(deferred_take_to_dst.m_dst.GetItem(), deferred_take_to_dst.m_dst, GameInventory.c_InventoryReservationTimeoutShortMS);
						EnableMovableOverride(deferred_take_to_dst.m_dst.GetItem());	
						InventoryInputUserData.SendInputUserDataMove(InventoryCommandType.SYNC_MOVE, deferred_take_to_dst.m_src, deferred_take_to_dst.m_dst);
					}
					else
					{
						#ifdef DEVELOPER
						if ( LogManager.IsInventoryMoveLogEnable() )
						{
							Debug.InventoryMoveLog("Can not move entity (JUNCTURE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item = " + deferred_take_to_dst.m_dst.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_take_to_dst.m_dst), "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );	
						}
						#endif
					}
					break;
				case InventoryMode.LOCAL:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("LOCAL ", "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );
					}
					#endif
					break;
				case InventoryMode.SERVER:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("SERVER ", "n/a" , "n/a", "HandleTakeToDst", GetDayZPlayerOwner().ToString() );
					}
					#endif
					break;

					break;
				default:
					Error("HandEvent - Invalid mode");
			}
		}
	}
	
	override bool SwapEntities (InventoryMode mode, notnull EntityAI item1, notnull EntityAI item2)
	{
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + item1 + " item2 = " + item2, "n/a" , "n/a", "SwapEntities", GetDayZPlayerOwner().ToString() );	
		}
		#endif
		
		InventoryLocation src1, src2, dst1, dst2;
		if( mode == InventoryMode.LOCAL )
		{
			if (GameInventory.MakeSrcAndDstForSwap(item1, item2, src1, src2, dst1, dst2))
			{
				LocationSwap(src1, src2, dst1, dst2);
				return true;
			}
		}
		
		if(!super.SwapEntities(mode,item1,item2))
		{
			if(!m_DeferredEvent)
			{
				if( GameInventory.MakeSrcAndDstForSwap(item1, item2, src1, src2, dst1, dst2) );
				{
					m_DeferredEvent = new DeferredSwapEntities(mode, item1, item2, dst1, dst2);
					if( m_DeferredEvent.ReserveInventory(this) )
						return true;
				}
			}
			m_DeferredEvent = null;
			return false;
		}
		return true;
	}
	
	void HandleSwapEntities( DeferredEvent deferred_event )
	{
		DeferredSwapEntities deferred_swap_entities = DeferredSwapEntities.Cast(deferred_event);
		if( deferred_swap_entities )
		{
			deferred_swap_entities.ClearInventoryReservation(this);
			InventoryLocation src1, src2, dst1, dst2;
			if (GameInventory.MakeSrcAndDstForSwap(deferred_swap_entities.m_item1, deferred_swap_entities.m_item2, src1, src2, dst1, dst2))
			{
				inventoryDebugPrint("[inv] I::Swap(" + typename.EnumToString(InventoryMode, deferred_swap_entities.m_mode) + ") src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2) +  " dst1=" + InventoryLocation.DumpToStringNullSafe(deferred_swap_entities.m_dst1) + " dst2=" + InventoryLocation.DumpToStringNullSafe(deferred_swap_entities.m_dst2));

				switch (deferred_swap_entities.m_mode)
				{
					case InventoryMode.PREDICTIVE:
						if (CanSwapEntitiesEx(deferred_swap_entities.m_dst1.GetItem(),deferred_swap_entities.m_dst2.GetItem()) )
						{
							InventoryInputUserData.SendInputUserDataSwap(src1, src2, deferred_swap_entities.m_dst1, deferred_swap_entities.m_dst2);
							LocationSwap(src1, src2, deferred_swap_entities.m_dst1, deferred_swap_entities.m_dst2);
						}
						else
						{
							#ifdef DEVELOPER
							if ( LogManager.IsInventoryMoveLogEnable() )
							{
								Debug.InventoryMoveLog("Can not swap (PREDICTIVE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + deferred_swap_entities.m_dst1.GetItem() + " item2 = " + deferred_swap_entities.m_dst2.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_swap_entities.m_dst2), "n/a" , "n/a", "ForceSwapEntities", GetDayZPlayerOwner().ToString() );	
							}
							#endif
						}
						break;
			
					case InventoryMode.JUNCTURE:
						if (CanSwapEntitiesEx(deferred_swap_entities.m_dst1.GetItem(),deferred_swap_entities.m_dst2.GetItem()) )
						{
							DayZPlayer player = GetGame().GetPlayer();
							player.GetHumanInventory().AddInventoryReservationEx(deferred_swap_entities.m_dst1.GetItem(), deferred_swap_entities.m_dst1, GameInventory.c_InventoryReservationTimeoutShortMS);
							player.GetHumanInventory().AddInventoryReservationEx(deferred_swap_entities.m_dst2.GetItem(), deferred_swap_entities.m_dst2, GameInventory.c_InventoryReservationTimeoutShortMS);
							EnableMovableOverride(deferred_swap_entities.m_dst1.GetItem());
							EnableMovableOverride(deferred_swap_entities.m_dst2.GetItem());
							InventoryInputUserData.SendInputUserDataSwap(src1, src2, deferred_swap_entities.m_dst1, deferred_swap_entities.m_dst2);
						}
						else
						{
							#ifdef DEVELOPER
							if ( LogManager.IsInventoryMoveLogEnable() )
							{
								Debug.InventoryMoveLog("Can not swap (JUNCTURE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + deferred_swap_entities.m_dst1.GetItem() + " item2 = " + deferred_swap_entities.m_dst2.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_swap_entities.m_dst2), "n/a" , "n/a", "ForceSwapEntities", GetDayZPlayerOwner().ToString() );	
							}
							#endif
						}
						break;

					case InventoryMode.LOCAL:
						break;

					default:
						Error("SwapEntities - HandEvent - Invalid mode");
				}
			}
			else
				Error("SwapEntities - MakeSrcAndDstForSwap - no inv loc");
		}
	}
	
	override bool ForceSwapEntities (InventoryMode mode, notnull EntityAI item1, notnull EntityAI item2, notnull InventoryLocation item2_dst)
	{
		#ifdef DEVELOPER
		if ( LogManager.IsInventoryMoveLogEnable() )
		{
			Debug.InventoryMoveLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + item1 + " item2 = " + item2 + " dst=" + InventoryLocation.DumpToStringNullSafe(item2_dst), "n/a" , "n/a", "ForceSwapEntities", GetDayZPlayerOwner().ToString() );	
		}
		#endif
		
		
		if( mode == InventoryMode.LOCAL )
		{
			InventoryLocation src1, src2, dst1;
			if (GameInventory.MakeSrcAndDstForForceSwap(item1, item2, src1, src2, dst1, item2_dst))
			{
				LocationSwap(src1, src2, dst1, item2_dst);
				return true;
			}
			
		}
		
		if(!super.ForceSwapEntities(mode,item1,item2,item2_dst))
		{
			if(!m_DeferredEvent)
			{
				if (GameInventory.MakeSrcAndDstForForceSwap(item1, item2, src1, src2, dst1, item2_dst))
				{
					m_DeferredEvent = new DeferredForceSwapEntities(mode,item1,item2, dst1, item2_dst);
					if( m_DeferredEvent.ReserveInventory(this))
						return true;

				}
			}
			m_DeferredEvent = null;
			return false;
		}
		
		return true;
	}

	void HandleForceSwapEntities( DeferredEvent deferred_event )
	{
		DeferredForceSwapEntities deferred_force_swap_entities = DeferredForceSwapEntities.Cast(deferred_event);
		if( deferred_force_swap_entities )
		{
			deferred_force_swap_entities.ClearInventoryReservation(this);
			InventoryLocation src1 = new InventoryLocation;
			InventoryLocation src2 = new InventoryLocation;
			deferred_force_swap_entities.m_item1.GetInventory().GetCurrentInventoryLocation(src1);
			deferred_force_swap_entities.m_item2.GetInventory().GetCurrentInventoryLocation(src2);

			DayZPlayer player = GetGame().GetPlayer();

			inventoryDebugPrint("[inv] I::FSwap(" + typename.EnumToString(InventoryMode, deferred_force_swap_entities.m_mode) + ") src1=" + InventoryLocation.DumpToStringNullSafe(src1) + " src2=" + InventoryLocation.DumpToStringNullSafe(src2) +  " dst1=" + InventoryLocation.DumpToStringNullSafe(deferred_force_swap_entities.m_dst1) + " dst2=" + InventoryLocation.DumpToStringNullSafe(deferred_force_swap_entities.m_dst2));

			switch (deferred_force_swap_entities.m_mode)
			{
				case InventoryMode.PREDICTIVE:
					if (CanForceSwapEntitiesEx(deferred_force_swap_entities.m_dst1.GetItem(),deferred_force_swap_entities.m_dst1,deferred_force_swap_entities.m_dst2.GetItem(), deferred_force_swap_entities.m_dst2))
					{
						InventoryInputUserData.SendInputUserDataSwap(src1, src2, deferred_force_swap_entities.m_dst1, deferred_force_swap_entities.m_dst2);
						LocationSwap(src1, src2, deferred_force_swap_entities.m_dst1, deferred_force_swap_entities.m_dst2);
					}
					else
					{
						#ifdef DEVELOPER
						if ( LogManager.IsInventoryMoveLogEnable() )
						{
							Debug.InventoryMoveLog("Can not force swap (PREDICTIVE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + deferred_force_swap_entities.m_dst1.GetItem() + " item2 = " + deferred_force_swap_entities.m_dst2.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_force_swap_entities.m_dst2), "n/a" , "n/a", "ForceSwapEntities", GetDayZPlayerOwner().ToString() );	
						}
						#endif						
					}
					break;

				case InventoryMode.JUNCTURE:
					if (CanForceSwapEntitiesEx(deferred_force_swap_entities.m_dst1.GetItem(),deferred_force_swap_entities.m_dst1,deferred_force_swap_entities.m_dst2.GetItem(), deferred_force_swap_entities.m_dst2))
					{	
						player.GetHumanInventory().AddInventoryReservationEx(deferred_force_swap_entities.m_item1, deferred_force_swap_entities.m_dst1, GameInventory.c_InventoryReservationTimeoutShortMS);
						player.GetHumanInventory().AddInventoryReservationEx(deferred_force_swap_entities.m_item2, deferred_force_swap_entities.m_dst2, GameInventory.c_InventoryReservationTimeoutShortMS);
				
						InventoryInputUserData.SendInputUserDataSwap(src1, src2, deferred_force_swap_entities.m_dst1, deferred_force_swap_entities.m_dst2);
					}
					else
					{
						#ifdef DEVELOPER
						if ( LogManager.IsInventoryMoveLogEnable() )
						{
							Debug.InventoryMoveLog("Can not force swap (JUNCTURE) STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp() + " item1 = " + deferred_force_swap_entities.m_dst1.GetItem() + " item2 = " + deferred_force_swap_entities.m_dst2.GetItem() + " dst=" + InventoryLocation.DumpToStringNullSafe(deferred_force_swap_entities.m_dst2), "n/a" , "n/a", "ForceSwapEntities", GetDayZPlayerOwner().ToString() );	
						}
						#endif
					}
					break;

				case InventoryMode.LOCAL:
					break;

				default:
					Error("ForceSwapEntities - HandEvent - Invalid mode");
			}
		}
	}
	
	static void SendServerHandEventViaJuncture (notnull DayZPlayer player, HandEventBase e)
	{
		if (GetGame().IsServer())
		{
			if (e.IsServerSideOnly())
				Error("[syncinv] " + Object.GetDebugName(player) + " SendServerHandEventViaJuncture - called on server side event only, e=" + e.DumpToString());
			
			if (player.IsAlive())
			{
				InventoryLocation dst = e.GetDst();
				InventoryLocation src = e.GetSrc();
				if (src.IsValid() && dst.IsValid())
				{
					if (player.NeedInventoryJunctureFromServer(src.GetItem(), src.GetParent(), dst.GetParent()))
					{
						syncDebugPrint("[syncinv] " + Object.GetDebugName(player) + " STS = " + player.GetSimulationTimeStamp() + " SendServerHandEventViaJuncture(" + typename.EnumToString(InventoryMode, InventoryMode.SERVER) + ") need juncture src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
						
						if (GetGame().AddInventoryJunctureEx(player, src.GetItem(), dst, true, GameInventory.c_InventoryReservationTimeoutMS))
							syncDebugPrint("[syncinv] " + Object.GetDebugName(player) + " STS = " + player.GetSimulationTimeStamp() + " SendServerHandEventViaJuncture(" + typename.EnumToString(InventoryMode, InventoryMode.SERVER) + ") got juncture");
						else
							syncDebugPrint("[syncinv] " + Object.GetDebugName(player) + " STS = " + player.GetSimulationTimeStamp() + " SendServerHandEventViaJuncture(" + typename.EnumToString(InventoryMode, InventoryMode.SERVER) + ") !got juncture");
					}
	
					syncDebugPrint("[syncinv] " + Object.GetDebugName(player) + " STS = " + player.GetSimulationTimeStamp() + " SendServerHandEventViaJuncture(" + typename.EnumToString(InventoryMode, InventoryMode.SERVER) + " server hand event src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
					
					ScriptInputUserData ctx = new ScriptInputUserData;
					InventoryInputUserData.SerializeHandEvent(ctx, e);
					player.SendSyncJuncture(DayZPlayerSyncJunctures.SJ_INVENTORY, ctx);
					syncDebugPrint("[syncinv] " + Object.GetDebugName(player) + " STS = " + player.GetSimulationTimeStamp() + " store input for remote - SendServerHandEventViaJuncture(" + typename.EnumToString(InventoryMode, InventoryMode.SERVER) + " server hand event src=" + InventoryLocation.DumpToStringNullSafe(src) + " dst=" + InventoryLocation.DumpToStringNullSafe(dst));
					player.StoreInputForRemotes(ctx); // @TODO: is this right place? maybe in HandleInputData(server=true, ...)
				}
			}
			else
			{
				Error("[syncinv] SendServerHandEventViaJuncture - called on dead player, juncture is for living only");
			}
		}
	}

	/**@fn			NetSyncCurrentStateID
	 * @brief		Engine callback - network synchronization of FSM's state. not intended to direct use.
	 **/
	override void NetSyncCurrentStateID (int id)
	{
		super.NetSyncCurrentStateID(id);

		GetDayZPlayerOwner().GetItemAccessor().OnItemInHandsChanged();
	}

	/**@fn			OnAfterStoreLoad
	 * @brief		engine reaction to load from database
	 *	originates in:
	 *				engine - Person::BinLoad
	 *				script - PlayerBase.OnAfterStoreLoad
	 **/
	override void OnAfterStoreLoad ()
	{
		GetDayZPlayerOwner().GetItemAccessor().OnItemInHandsChanged(true);
	}

	/**@fn			OnEventForRemoteWeapon
	 * @brief		reaction of remote weapon to (common user/anim/...) event sent from server
	 **/
	bool OnEventForRemoteWeapon (ParamsReadContext ctx)
	{
		if (GetEntityInHands())
		{
			Weapon_Base wpn = Weapon_Base.Cast(GetEntityInHands());
			if (wpn)
			{
				PlayerBase pb = PlayerBase.Cast(GetDayZPlayerOwner());

				WeaponEventBase e = CreateWeaponEventFromContext(ctx);
				if (pb && e)
				{
					pb.GetWeaponManager().SetRunning(true);
		
					fsmDebugSpam("[wpnfsm] " + Object.GetDebugName(wpn) + " recv event from remote: created event=" + e);
					if (e.GetEventID() == WeaponEventID.HUMANCOMMAND_ACTION_ABORTED)
					{
						wpn.ProcessWeaponAbortEvent(e);
					}
					else
					{
						wpn.ProcessWeaponEvent(e);
					}
					pb.GetWeaponManager().SetRunning(false);
				}
			}
			else
				Error("OnEventForRemoteWeapon - entity in hands, but not weapon. item=" + GetEntityInHands());
		}
		else
			Error("OnEventForRemoteWeapon - no entity in hands");
		return true;
	}


	/**@fn			OnHandEventForRemote
	 * @brief		reaction of remote weapon to (common user/anim/...) event sent from server
	 **/
	bool OnHandEventForRemote (ParamsReadContext ctx)
	{
		HandEventBase e = HandEventBase.CreateHandEventFromContext(ctx);
		if (e)
		{
			hndDebugSpam("[hndfsm] recv event from remote: created event=" + e);
			//m_FSM.ProcessEvent(e);
			if (e.GetEventID() == HandEventID.HUMANCOMMAND_ACTION_ABORTED)
			{
				ProcessEventResult aa;
				m_FSM.ProcessAbortEvent(e, aa);
			}
			else
			{
				m_FSM.ProcessEvent(e);
			}

			return true;
		}
		return false;
	}

	void SyncHandEventToRemote (HandEventBase e)
	{
		DayZPlayer p = GetDayZPlayerOwner();
		if (p && p.GetInstanceType() == DayZPlayerInstanceType.INSTANCETYPE_SERVER)
		{
			ScriptRemoteInputUserData ctx = new ScriptRemoteInputUserData;

			ctx.Write(INPUT_UDT_HAND_REMOTE_EVENT);
			e.WriteToContext(ctx);

			hndDebugPrint("[hndfsm] send 2 remote: sending e=" + e + " id=" + e.GetEventID() + " p=" + p + "  e=" + e.DumpToString());
			p.StoreInputForRemotes(ctx);
		}
	}
	
	override void OnHandsExitedStableState (HandStateBase src, HandStateBase dst)
	{
		super.OnHandsExitedStableState(src, dst);

		hndDebugPrint("[hndfsm] hand fsm exit stable src=" + src.Type().ToString());
	}
	
	override void OnHandsEnteredStableState (HandStateBase src, HandStateBase dst)
	{
		super.OnHandsEnteredStableState(src, dst);

		hndDebugPrint("[hndfsm] hand fsm entered stable dst=" + dst.Type().ToString());
	}
	
	override void OnHandsStateChanged (HandStateBase src, HandStateBase dst)
	{
		super.OnHandsStateChanged(src, dst);

		hndDebugPrint("[hndfsm] hand fsm changed state src=" + src.Type().ToString() + " ---> dst=" + dst.Type().ToString());

		if (src.IsIdle())
			OnHandsExitedStableState(src, dst);
		
		if (dst.IsIdle())
			OnHandsEnteredStableState(src, dst);

#ifdef BOT
		PlayerBase p = PlayerBase.Cast(GetDayZPlayerOwner());
		if (p && p.m_Bot)
		{
			p.m_Bot.ProcessEvent(new BotEventOnItemInHandsChanged(p));
		}
#endif
	}
	
	override bool HandEvent(InventoryMode mode, HandEventBase e)
	{
		if (!IsProcessing())
		{
			EntityAI itemInHands = GetEntityInHands();
			
			InventoryLocation handInventoryLocation();
			handInventoryLocation.SetHands(GetInventoryOwner(), itemInHands);
			
			InventoryValidation validation();
			if (e.CanPerformEventEx(validation))
			{
				m_DeferredEvent = new DeferredHandEvent(mode, e);
				if (m_DeferredEvent.ReserveInventory(this))
				{
					return true;
				}
			}
			
			m_DeferredEvent = null;

			//! Let the client know a failure happened
			if (!GetGame().IsMultiplayer() || GetGame().IsClient())
			{
				//! If singleplayer or the client is executing this
				OnInventoryFailure(InventoryCommandType.HAND_EVENT, validation.m_Reason, e.GetSrc(), e.GetDst());
			}
			else
			{
				ScriptInputUserData serializer();

				InventoryInputUserData.SerializeHandEvent(serializer, e);
				serializer.Write(validation.m_Reason);

				GetDayZPlayerOwner().SendSyncJuncture(DayZPlayerSyncJunctures.SJ_INVENTORY_FAILURE, serializer);
			}
		}
		
		return false;
	}
	
	
	void HandleHandEvent(DeferredEvent deferred_event)
	{
		//! Default structure suffices
		InventoryValidation validation();
		
		DeferredHandEvent deferred_hand_event = DeferredHandEvent.Cast(deferred_event);
		if (deferred_hand_event)
		{
			#ifdef DEVELOPER
			if ( LogManager.IsInventoryHFSMLogEnable() )
			{	
				Debug.InventoryHFSMLog("STS = " + GetDayZPlayerOwner().GetSimulationTimeStamp(), "n/a" , "n/a", "HandleHandEvent", GetDayZPlayerOwner().ToString() );
			}
			#endif
			
			
			hndDebugPrint("[inv] HumanInventory::HandEvent(" + typename.EnumToString(InventoryMode, deferred_hand_event.m_mode) + ") ev=" + deferred_hand_event.m_event.DumpToString());

			switch (deferred_hand_event.m_mode)
			{
				case InventoryMode.PREDICTIVE:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("PREDICTIVE", "n/a" , "n/a", "HandleHandEvent", GetDayZPlayerOwner().ToString() );
					}
					#endif
					deferred_hand_event.ClearInventoryReservation(this);
					if (deferred_hand_event.m_event.CanPerformEventEx(validation))
					{
						InventoryInputUserData.SendInputUserDataHandEvent(deferred_hand_event.m_event);
						ProcessHandEvent(deferred_hand_event.m_event);
					}
					break;

				case InventoryMode.JUNCTURE:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("JUNCTURE", "n/a" , "n/a", "HandleHandEvent", GetDayZPlayerOwner().ToString() );
					}
					#endif
					deferred_hand_event.ClearInventoryReservation(this);
					if (deferred_hand_event.m_event.CanPerformEventEx(validation))
					{
						deferred_hand_event.ReserveInventory(this);
						InventoryInputUserData.SendInputUserDataHandEvent(deferred_hand_event.m_event);
					
						//Functionality to prevent desync when two players perform interfering action at the same time
						EntityAI itemSrc = deferred_hand_event.m_event.GetSrcEntity();
						EntityAI itemDst = null;
						if (deferred_hand_event.m_event.GetDst())
							itemDst = deferred_hand_event.m_event.GetDst().GetItem();
						if (itemSrc)
							EnableMovableOverride(itemSrc);
						if (itemDst)
							EnableMovableOverride(itemDst);
					}
					break;

				case InventoryMode.LOCAL:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("LOCAL", "n/a" , "n/a", "HandleHandEvent", GetDayZPlayerOwner().ToString() );
					}
					#endif
					deferred_hand_event.ClearInventoryReservation(this);
					ProcessHandEvent(deferred_hand_event.m_event);
					//PostHandEvent(deferred_hand_event.m_event);
					break;
			
				case InventoryMode.SERVER:
					#ifdef DEVELOPER
					if ( LogManager.IsInventoryHFSMLogEnable() )
					{	
						Debug.InventoryHFSMLog("SERVER", "n/a" , "n/a", "HandleHandEvent", GetDayZPlayerOwner().ToString() );
					}
					#endif
					hndDebugPrint("[inv] DZPI::HandEvent(" + typename.EnumToString(InventoryMode, deferred_hand_event.m_mode) + ")");
					if (!deferred_hand_event.m_event.IsServerSideOnly())
					{
						if (GetDayZPlayerOwner().IsAlive())
						{
							SendServerHandEventViaJuncture(GetDayZPlayerOwner(), deferred_hand_event.m_event);
						}
						else
						{
							InventoryInputUserData.SendServerHandEventViaInventoryCommand(GetDayZPlayerOwner(), deferred_hand_event.m_event);
						}
					}
					else
					{
						ProcessHandEvent(deferred_hand_event.m_event);
					}
					break;

				default:
					Error("HumanInventory::HandEvent - Invalid mode");
			}
		}
	}
	
	override void HandleInventoryManipulation()
	{
		super.HandleInventoryManipulation();
		if(m_DeferredEvent && ScriptInputUserData.CanStoreInputUserData() )
		{
			HandleHandEvent(m_DeferredEvent);
			HandleTakeToDst(m_DeferredEvent);
			HandleSwapEntities(m_DeferredEvent);
			HandleForceSwapEntities(m_DeferredEvent);
		
			m_DeferredEvent = null;
		}
	}
	
	
	
	bool IsProcessing()
	{
		return !m_FSM.GetCurrentState().IsIdle() || m_DeferredEvent || m_DeferredPostedHandEvent;
	}
	
	bool PlayerCheckRequestSrc ( notnull InventoryLocation src, float radius )
	{
		bool result = true;
		
		EntityAI ent = src.GetParent();
		if ( ent )
		{
			PlayerBase player = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
			if (player)
			{
				if ( GetDayZPlayerOwner() != player )
				{
					if (player.IsAlive())
					{
						if (!player.IsRestrained() && !player.IsUnconscious())
						{
							return false;
						}
					}
				}
			}
		}
		
		if ( result )
		{
			result = CheckRequestSrc( GetManOwner(), src, radius);
		}
		
		return result;
	}

	bool PlayerCheckRequestDst ( notnull InventoryLocation src, notnull InventoryLocation dst, float radius )
	{
		bool result = true;
		
		EntityAI ent = dst.GetParent();
		if ( ent )
		{
			PlayerBase player = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
			if (player)
			{
				if ( GetDayZPlayerOwner() != player )
				{
					if (player.IsAlive())
					{
						if (!player.IsRestrained() && !player.IsUnconscious())
						{
							return false;
						}
					}
				}
			}
		}
		
		if ( result )
		{
			result = CheckMoveToDstRequest( GetManOwner(), src, dst, radius);
		}
		
		return result;
	}	
	
	bool PlayerCheckSwapItemsRequest( notnull InventoryLocation src1,  notnull InventoryLocation src2, notnull InventoryLocation dst1, notnull InventoryLocation dst2, float radius)
	{
		bool result = true;
		
		EntityAI ent = dst1.GetParent();
		PlayerBase player;
		if ( ent )
		{
			player = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
			if (player)
			{
				if ( GetDayZPlayerOwner() != player )
				{
					if (player.IsAlive())
					{
						if (!player.IsRestrained() && !player.IsUnconscious())
						{
							return false;
						}
					}
				}
			}
		}
		
		ent = dst2.GetParent();
		if ( ent )
		{
			player = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
			if (player)
			{
				if ( GetDayZPlayerOwner() != player )
				{
					if (player.IsAlive())
					{
						if (!player.IsRestrained() && !player.IsUnconscious())
						{
							return false;
						}
					}
				}
			}
		}
		
		
		if ( result )
		{
			result = CheckSwapItemsRequest( GetManOwner(), src1, src2, dst1, dst2, GameInventory.c_MaxItemDistanceRadius);
		}
		
		return result;
		
	}
	
	bool PlayerCheckDropRequest ( notnull InventoryLocation src, float radius )
	{
		bool result = true;
		
		EntityAI ent = src.GetParent();
		if ( ent )
		{
			PlayerBase player = PlayerBase.Cast(ent.GetHierarchyRootPlayer());
			if (player)
			{
				if ( GetDayZPlayerOwner() != player )
				{
					if (player.IsAlive())
					{
						if (!player.IsRestrained() && !player.IsUnconscious())
						{
							return false;
						}
					}
				}
			}
		}
		
		if ( result )
		{
			result = CheckDropRequest( GetManOwner(), src, radius);
		}
		
		return result;
	}
};

