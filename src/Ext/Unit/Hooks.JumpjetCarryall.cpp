#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Unit/Body.h>
#include <Utilities/Detach.h>

#include <TacticalClass.h>

// Hooks for the jumpjet carryall. See Ext/Unit/Body.JumpjetCarryall.cpp for the mechanic itself.

namespace
{
	// Keeps the carrier/payload and carrier/target links from dangling. Registered once
	// for FootClass, so a single lookup resolves both sides through the extension
	// back-pointers instead of walking every carrier on the map.
	class JumpjetCarryallDetachListener final : public Detach::Listener<FootClass>
	{
	public:
		void OnDetach(FootClass* pTarget, bool removed) override
		{
			// A false flag only means the object left the map, which is exactly the state a
			// slung payload is kept in on purpose. Only a real removal breaks the links.
			if (!removed)
				return;

			auto const pExt = FootExt::TryFetch(pTarget);

			if (!pExt)
				return;

			if (auto const pCarrier = pExt->JumpjetCarryall_Carrier)
			{
				pExt->JumpjetCarryall_Carrier = nullptr;

				if (auto const pCarrierExt = UnitExt::TryFetch(pCarrier))
					pCarrierExt->OnJumpjetCarryallDetach(pTarget);
			}

			if (auto const pCarrier = pExt->JumpjetCarryall_TargetedBy)
			{
				pExt->JumpjetCarryall_TargetedBy = nullptr;

				if (auto const pCarrierExt = UnitExt::TryFetch(pCarrier))
					pCarrierExt->OnJumpjetCarryallDetach(pTarget);
			}
		}
	};

	JumpjetCarryallDetachListener JumpjetCarryallDetach {};
}

// Show the tote cursor when a carryall jumpjet hovers over a unit it could lift.
DEFINE_HOOK(0x74041B, UnitClass_WhatAction_JumpjetCarryall, 0x5)
{
	enum { Attack = 0x740420, Other = 0x74043B };

	GET(Action, action, EBX);
	GET(UnitClass*, pThis, ESI);
	GET(TechnoClass*, pTarget, EDI);

	if (action == Action::Select && pTarget != pThis && !pTarget->IsInAir())
	{
		if (UnitExt::Fetch(pThis)->CanLiftJumpjetCargo(pTarget))
		{
			action = Action::Tote;
			R->EBX(action);
		}
	}

	return action == Action::Attack ? Attack : Other;
}

// Show the deploy cursor over a loaded carrier that has no passenger bays of its own.
DEFINE_HOOK(0x74000B, UnitClass_WhatAction_JumpjetCarryall_Drop, 0x6)
{
	enum { SelfDeploy = 0x7400FA };

	GET(UnitClass*, pThis, ESI);

	return UnitExt::Fetch(pThis)->JumpjetCarryall_Payload ? SelfDeploy : 0;
}

// Turn a tote click into a pickup mission.
DEFINE_HOOK(0x7388FD, UnitClass_ActiveClickWith_JumpjetCarryall, 0x5)
{
	GET(const Action, action, ECX);
	GET(UnitClass*, pThis, ESI);
	GET(TechnoClass*, pTarget, EDI);

	if (action == Action::Tote)
	{
		if (auto const pTargetFoot = abstract_cast<FootClass*>(pTarget))
			UnitExt::Fetch(pThis)->StartJumpjetCarryallMission(pTargetFoot);
	}
	else
	{
		// Any other order the player gives replaces a running pickup.
		auto const pExt = UnitExt::Fetch(pThis);

		if (pExt->JumpjetCarryall_State >= JumpjetCarryallState::Approach)
			pExt->CancelJumpjetCarryallMission(true);
	}

	return 0;
}

// A loaded carrier may always deploy, even without passenger bays, so it can set its cargo down.
DEFINE_HOOK(0x700E8B, TechnoClass_CanDeploy_JumpjetCarryall, 0x6)
{
	enum { CheckLandType = 0x700EEC };

	GET(TechnoTypeClass*, pType, EAX);

	if (pType->Passengers > 0)
		return 0;

	GET(TechnoClass*, pThis, ESI);

	if (pThis->WhatAmI() != AbstractType::Unit)
		return 0;

	auto const pExt = UnitExt::Fetch(static_cast<UnitClass*>(pThis));

	return pExt->JumpjetCarryall_Payload && pExt->IsJumpjetCarryall() ? CheckLandType : 0;
}

// Deploying a loaded carrier releases its cargo.
DEFINE_HOOK(0x73D6EC, UnitClass_Mission_Unload_JumpjetCarryall, 0x6)
{
	enum { UnloadPassengers = 0x73D772 };

	GET(UnitClass*, pThis, ESI);

	auto const pExt = UnitExt::Fetch(pThis);

	if (!pExt->JumpjetCarryall_Payload)
		return 0;

	pExt->DropJumpjetCarryallPayload();

	const bool noPassengerBays = pThis->Type->Passengers <= 0;

	if (noPassengerBays)
		pThis->QueueMission(Mission::Guard, false);

	return pThis->Passengers.FirstPassenger || noPassengerBays ? UnloadPassengers : 0;
}

// Draw the slung unit under its carrier - the payload is in limbo, so nothing else draws it.
DEFINE_HOOK(0x73D317, UnitClass_DrawIt_JumpjetCarryall, 0x6)
{
	GET(UnitClass*, pThis, ESI);

	auto const pPayload = UnitExt::Fetch(pThis)->JumpjetCarryall_Payload;

	if (!pPayload || !pPayload->IsAlive)
		return 0;

	GET(RectangleStruct*, pBounds, EBX);
	GET_STACK(const int, x, 0x1C);
	GET_STACK(const int, y, 0x20);

	Point2D point { x, y };

	const auto offset = TechnoTypeExt::Fetch(pPayload->GetTechnoType())->JumpjetCarryall_Offset.Get();

	if (offset != CoordStruct::Empty)
		point += TacticalClass::CoordsToScreen(TechnoExt::GetFLHAbsoluteCoords(pThis, offset) - pThis->GetCoords());

	pPayload->DrawIt(&point, pBounds);

	return 0;
}
