#include "Body.h"

#include <JumpjetLocomotionClass.h>

#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>

// Jumpjet carryall.
//
// A VehicleType that uses the jumpjet locomotor and sets JumpjetCarryall=yes can
// sling a single ground unit underneath itself, the same way a vanilla aircraft
// carryall does. The player targets a unit with the Tote cursor, the carrier flies
// over it, lowers itself onto it, secures it, and releases it again with the
// deploy/unload command.
//
// The whole mission is driven from UnitClass::AI, so the carrier keeps its normal
// jumpjet movement, weapons and mission handling while the pickup runs. Descending
// and climbing are done by handing a new target height to the jumpjet locomotor
// rather than by moving the carrier by hand, which keeps the engine in charge of
// the actual flight.

namespace
{
	// Height (in leptons) below which the sling can reach a grounded unit.
	constexpr int PickupReachHeight = Unsorted::LevelHeight;

	int GetCruiseHeight(UnitClass* pThis)
	{
		return pThis->Type->JumpjetHeight;
	}

	// Frame budget for a phase that waits on the locomotor to change the carrier's
	// altitude, sized from the rate that phase actually runs at. Generous multiple of
	// the ideal time so that a slow jumpjet is never cut short, while a carrier that
	// physically cannot get where it needs to go still gives up.
	int GetHeightChangeBudget(UnitClass* pThis, double rate)
	{
		const int height = Math::max(GetCruiseHeight(pThis), Unsorted::LevelHeight);

		return static_cast<int>(height / Math::max(rate, 0.05)) * 4 + 150;
	}

	// Rate the carrier actually lowers itself at, which JumpjetCarryall.DescendRate is
	// free to make much slower than JumpjetClimb. Kept fractional so that a jumpjet with
	// a sub-1 JumpjetClimb is not budgeted as if it climbed a whole lepton per frame.
	double GetDescendRate(UnitClass* pThis, UnitTypeExt* pTypeExt)
	{
		const int rate = pTypeExt->JumpjetCarryall_DescendRate;

		return rate > 0 ? static_cast<double>(rate) : static_cast<double>(pThis->Type->JumpjetClimb);
	}

	// Frame budget for the flight to the target's cell. This is a no-progress allowance,
	// not a total flight time: it only runs down while the carrier fails to get any
	// closer, so the range of a pickup is not capped.
	constexpr int StallBudget = 450;

	// Budget for a whole pickup. Approach and descend share it, because the carrier can
	// bounce between those two phases as the target shuffles around underneath it and a
	// per-phase budget would then never run out.
	int GetPickupBudget(UnitClass* pThis, UnitTypeExt* pTypeExt)
	{
		return StallBudget + GetHeightChangeBudget(pThis, GetDescendRate(pThis, pTypeExt));
	}

	int GetCellDistance(const CellStruct& a, const CellStruct& b)
	{
		const int dx = a.X - b.X;
		const int dy = a.Y - b.Y;

		return Math::max(dx < 0 ? -dx : dx, dy < 0 ? -dy : dy);
	}

	bool IsListed(const std::vector<TechnoTypeClass*>& list, TechnoTypeClass* pType)
	{
		return std::find(list.begin(), list.end(), pType) != list.end();
	}
}

bool UnitExt::IsJumpjetCarryall() const
{
	auto const pThis = this->OwnerObject();

	if (!this->GetTypeExtData()->JumpjetCarryall)
		return false;

	return locomotion_cast<JumpjetLocomotionClass*>(pThis->Locomotor) != nullptr;
}

bool UnitExt::IsJumpjetCarryallLandingOnTarget() const
{
	// Approach counts too: the state machine runs after the locomotor, so it is always a
	// frame behind, and a pickup ordered on a unit already sharing the carrier's cell can
	// reach the locomotor's descent before the state flips to Descend.
	if (this->JumpjetCarryall_State != JumpjetCarryallState::Descend
		&& this->JumpjetCarryall_State != JumpjetCarryallState::Approach)
	{
		return false;
	}

	auto const pTarget = this->JumpjetCarryall_Target;

	return pTarget && this->OwnerObject()->GetMapCoords() == pTarget->GetMapCoords();
}

double UnitExt::GetJumpjetCarryallSpeedMultiplier() const{
	if (!this->JumpjetCarryall_Payload)
		return 1.0;

	return this->GetTypeExtData()->JumpjetCarryall_SpeedMultiplier;
}

bool UnitExt::CanLiftJumpjetCargo(TechnoClass* pTarget) const
{
	auto const pThis = this->OwnerObject();

	if (!pTarget || pTarget == pThis || this->JumpjetCarryall_Payload || !this->IsJumpjetCarryall())
		return false;

	auto const pTargetFoot = abstract_cast<FootClass*>(pTarget);

	if (!pTargetFoot || !pTargetFoot->IsAlive || pTargetFoot->InLimbo || pTargetFoot->IsInAir())
		return false;

	// A unit can only hang under one carrier, and one already inside a transport is off limits.
	if (pTargetFoot->Transporter || FootExt::Fetch(pTargetFoot)->JumpjetCarryall_Carrier)
		return false;

	// Two carriers must not race for the same unit.
	auto const pClaimedBy = FootExt::Fetch(pTargetFoot)->JumpjetCarryall_TargetedBy;

	if (pClaimedBy && pClaimedBy != pThis)
		return false;

	// Anything that pins the target down also blocks a pickup.
	if (pTargetFoot->ParasiteEatingMe || pTargetFoot->IsIronCurtained() || pTargetFoot->WarpingOut
		|| pTargetFoot->BeingWarpedOut || pTargetFoot->IsImmobilized || pTargetFoot->IsAttackedByLocomotor)
		return false;

	auto const pTypeExt = this->GetTypeExtData();

	switch (pTargetFoot->WhatAmI())
	{
	case AbstractType::Infantry:
		if (!pTypeExt->JumpjetCarryall_AllowInfantry)
			return false;
		break;

	case AbstractType::Unit:
		if (!pTypeExt->JumpjetCarryall_AllowVehicles)
			return false;
		break;

	default:
		return false;
	}

	// Own units are always fair game, allied ones only when the carrier allows it.
	if (pThis->Owner != pTargetFoot->Owner
		&& !(pTypeExt->JumpjetCarryall_AllowAllied && pThis->Owner->IsAlliedWith(pTargetFoot->Owner)))
	{
		return false;
	}

	auto const pTargetType = pTargetFoot->GetTechnoType();

	if (!TechnoTypeExt::Fetch(pTargetType)->JumpjetCarryall_Allowed.Get(true))
		return false;

	const int sizeLimit = pTypeExt->JumpjetCarryall_SizeLimit;

	if (sizeLimit >= 0 && pTargetType->Size > sizeLimit)
		return false;

	if (IsListed(pTypeExt->JumpjetCarryall_DisallowedTypes, pTargetType))
		return false;

	if (!pTypeExt->JumpjetCarryall_AllowedTypes.empty()
		&& !IsListed(pTypeExt->JumpjetCarryall_AllowedTypes, pTargetType))
	{
		return false;
	}

	return true;
}

bool UnitExt::TickJumpjetCarryallPickup()
{
	auto const pThis = this->OwnerObject();
	auto const pTypeExt = this->GetTypeExtData();

	// The watchdog measures lack of progress, not elapsed time, so a pickup ordered across
	// the whole map is fine as long as the carrier keeps closing in. Only a strictly new
	// record counts: a target jittering back and forth must not keep resetting the clock.
	const int distance = GetCellDistance(pThis->GetMapCoords(), this->JumpjetCarryall_TargetCell);

	if (distance < this->JumpjetCarryall_BestDistance)
	{
		this->JumpjetCarryall_BestDistance = distance;
		this->JumpjetCarryall_Timer = 0;

		return true;
	}

	return ++this->JumpjetCarryall_Timer <= GetPickupBudget(pThis, pTypeExt);
}

void UnitExt::StartJumpjetCarryallMission(FootClass* pTarget)
{
	auto const pThis = this->OwnerObject();

	if (!this->CanLiftJumpjetCargo(pTarget))
		return;

	auto const pCell = MapClass::Instance.TryGetCellAt(pTarget->GetCoords());

	if (!pCell)
		return;

	// Drop the claim on whatever the carrier was after before, or that unit stays
	// permanently off limits to every other carryall.
	this->CancelJumpjetCarryallMission(true);

	this->JumpjetCarryall_Target = pTarget;
	this->JumpjetCarryall_TargetCell = pCell->MapCoords;
	this->JumpjetCarryall_State = JumpjetCarryallState::Approach;
	this->JumpjetCarryall_Timer = 0;
	this->JumpjetCarryall_BestDistance = INT_MAX;
	FootExt::Fetch(pTarget)->JumpjetCarryall_TargetedBy = pThis;

	pThis->SetTarget(nullptr);
	pThis->SetDestination(pCell, true);
	pThis->QueueMission(Mission::Move, true);
}

void UnitExt::CancelJumpjetCarryallMission(bool keepReady)
{
	auto const pThis = this->OwnerObject();

	if (auto const pTarget = this->JumpjetCarryall_Target)
	{
		auto const pTargetExt = FootExt::TryFetch(pTarget);

		if (pTargetExt && pTargetExt->JumpjetCarryall_TargetedBy == pThis)
			pTargetExt->JumpjetCarryall_TargetedBy = nullptr;
	}

	this->JumpjetCarryall_Target = nullptr;
	this->JumpjetCarryall_TargetCell = CellStruct::Empty;
	this->JumpjetCarryall_State = keepReady ? JumpjetCarryallState::Ready : JumpjetCarryallState::Inactive;
	this->JumpjetCarryall_Timer = 0;
	this->JumpjetCarryall_BestDistance = INT_MAX;

	// Hand the cruise height back to the locomotor.
	if (auto const pJJLoco = locomotion_cast<JumpjetLocomotionClass*>(pThis->Locomotor))
	{
		pJJLoco->CurrentHeight = GetCruiseHeight(pThis);
		pJJLoco->Climb = static_cast<float>(pThis->Type->JumpjetClimb);
	}
}

void UnitExt::UpdateJumpjetCarryall()
{
	auto const pThis = this->OwnerObject();

	if (!this->IsJumpjetCarryall())
	{
		// The locomotor may have been swapped out from under a loaded carrier.
		if (this->JumpjetCarryall_Payload)
			this->DropJumpjetCarryallPayload();

		return;
	}

	auto const pJJLoco = locomotion_cast<JumpjetLocomotionClass*>(pThis->Locomotor);
	auto const pTypeExt = this->GetTypeExtData();

	// Keep the slung unit glued to the carrier.
	if (auto const pPayload = this->JumpjetCarryall_Payload)
	{
		if (!pPayload->IsAlive)
		{
			this->JumpjetCarryall_Payload = nullptr;
		}
		else
		{
			// Limbo() is what keeps the payload off the map; re-apply it if anything undid it.
			if (!pPayload->InLimbo)
				pPayload->Limbo();

			pPayload->SetLocation(pThis->Location);
			pPayload->OnBridge = false;
			pPayload->IsOnCarryall = true;

			const auto facing = pThis->PrimaryFacing.Current();
			pPayload->PrimaryFacing.SetCurrent(facing);
			pPayload->SecondaryFacing.SetCurrent(facing);

			if (auto const pPayloadLoco = locomotion_cast<JumpjetLocomotionClass*>(pPayload->Locomotor))
				pPayloadLoco->LocomotionFacing.SetCurrent(facing);
		}
	}

	// Carrying something slows the carrier down. That factor is folded into
	// TechnoExt::GetCurrentSpeedMultiplier so it composes with every other speed modifier
	// instead of fighting the locomotor over the raw speed value.

	if (!pThis->IsAlive || pThis->InLimbo)
		return;

	switch (this->JumpjetCarryall_State)
	{
	case JumpjetCarryallState::Inactive:
	{
		if (!this->JumpjetCarryall_Payload && pThis->IsInAir())
			this->JumpjetCarryall_State = JumpjetCarryallState::Ready;

		break;
	}

	case JumpjetCarryallState::Ready:
	{
		if (this->JumpjetCarryall_Payload || !pThis->IsInAir())
			this->JumpjetCarryall_State = JumpjetCarryallState::Inactive;

		break;
	}

	case JumpjetCarryallState::Approach:
	{
		auto const pTarget = this->JumpjetCarryall_Target;

		if (!this->CanLiftJumpjetCargo(pTarget) || !this->TickJumpjetCarryallPickup())
		{
			this->CancelJumpjetCarryallMission(true);
			break;
		}

		auto const pCell = MapClass::Instance.TryGetCellAt(pTarget->GetCoords());

		if (!pCell)
		{
			this->CancelJumpjetCarryallMission(true);
			break;
		}

		if (pThis->GetMapCoords() == pCell->MapCoords)
		{
			this->JumpjetCarryall_State = JumpjetCarryallState::Descend;
			break;
		}

		// Keep the carrier aimed at the target's cell. A dropped destination is not the
		// player calling the pickup off - the engine readily drops one it considers
		// unreachable, and an occupied cell normally is one - so the order is re-issued.
		// A destination pointing somewhere else is someone else's order, and the carrier
		// is handed over to it: a right click on open ground goes through
		// CellClickedAction, which the carryall click hook does not see.
		if (pCell->MapCoords != this->JumpjetCarryall_TargetCell || !pThis->Destination)
		{
			this->JumpjetCarryall_TargetCell = pCell->MapCoords;
			pThis->SetDestination(pCell, true);
			pThis->QueueMission(Mission::Move, true);
		}
		else if (pThis->Destination != pCell)
		{
			this->CancelJumpjetCarryallMission(true);
		}

		break;
	}

	case JumpjetCarryallState::Descend:
	{
		auto const pTarget = this->JumpjetCarryall_Target;

		if (!this->CanLiftJumpjetCargo(pTarget) || !this->TickJumpjetCarryallPickup())
		{
			this->CancelJumpjetCarryallMission(true);
			break;
		}

		// Target slipped out from under us: climb again and resume the chase.
		if (pThis->GetMapCoords() != pTarget->GetMapCoords())
		{
			pJJLoco->CurrentHeight = GetCruiseHeight(pThis);
			pJJLoco->Climb = static_cast<float>(pThis->Type->JumpjetClimb);
			this->JumpjetCarryall_State = JumpjetCarryallState::Approach;
			break;
		}

		// Someone else sent the carrier somewhere - hand it over. A null destination is
		// not a takeover here: arriving over the target clears it.
		if (pThis->Destination
			&& CellClass::Coord2Cell(pThis->Destination->GetCoords()) != this->JumpjetCarryall_TargetCell)
		{
			this->CancelJumpjetCarryallMission(true);
			break;
		}

		// Let the locomotor lower the carrier at its own rate.
		const int descendRate = pTypeExt->JumpjetCarryall_DescendRate;

		if (descendRate > 0)
			pJJLoco->Climb = static_cast<float>(descendRate);

		pJJLoco->CurrentHeight = 0;

		if (pThis->GetHeight() > PickupReachHeight)
			break;

		// Close enough - sling the target.
		auto const pTargetExt = FootExt::Fetch(pTarget);

		pTarget->Deselect();
		pTarget->Limbo();
		pTarget->SetLocation(pThis->Location);
		pTarget->OnBridge = false;
		pTarget->IsOnCarryall = true;

		this->JumpjetCarryall_Payload = pTarget;
		pTargetExt->JumpjetCarryall_Carrier = pThis;
		pTargetExt->JumpjetCarryall_TargetedBy = nullptr;

		this->JumpjetCarryall_Target = nullptr;
		this->JumpjetCarryall_TargetCell = CellStruct::Empty;

		const int sound = pTypeExt->JumpjetCarryall_PickupSound.Get(pThis->Type->EnterTransportSound);

		if (sound != -1)
			VocClass::PlayAt(sound, pThis->Location);

		pJJLoco->Climb = static_cast<float>(pThis->Type->JumpjetClimb);
		pJJLoco->CurrentHeight = GetCruiseHeight(pThis);
		this->JumpjetCarryall_State = JumpjetCarryallState::Ascend;
		this->JumpjetCarryall_Timer = 0;
		this->JumpjetCarryall_BestDistance = INT_MAX;

		break;
	}

	case JumpjetCarryallState::Ascend:
	{
		// A jumpjet that does not hover when idle has nothing to climb back to: it parks
		// itself on the ground exactly like it would without any cargo, and takes off
		// again with the cargo the next time it is given somewhere to go. Forcing a
		// cruise height on it here would only fight the locomotor's own landing.
		if (!pThis->Type->BalloonHover && !pThis->Destination
			&& pJJLoco->State == JumpjetLocomotionClass::State::Grounded)
		{
			this->JumpjetCarryall_State = JumpjetCarryallState::Inactive;
			break;
		}

		const int cruiseHeight = GetCruiseHeight(pThis);
		pJJLoco->CurrentHeight = cruiseHeight;

		if (pThis->GetHeight() >= cruiseHeight - Unsorted::LevelHeight
			|| ++this->JumpjetCarryall_Timer
				> GetHeightChangeBudget(pThis, pThis->Type->JumpjetClimb))
		{
			this->JumpjetCarryall_State = JumpjetCarryallState::Inactive;
		}

		break;
	}
	}
}

bool UnitExt::DropJumpjetCarryallPayload()
{
	auto const pThis = this->OwnerObject();
	auto const pPayload = this->JumpjetCarryall_Payload;

	if (!pPayload)
		return false;

	auto const pPayloadExt = FootExt::Fetch(pPayload);

	this->JumpjetCarryall_Payload = nullptr;
	pPayloadExt->JumpjetCarryall_Carrier = nullptr;
	pPayload->IsOnCarryall = false;

	this->CancelJumpjetCarryallMission();

	if (!pPayload->IsAlive)
		return false;

	auto const pPayloadType = pPayload->GetTechnoType();
	auto coords = pThis->Location;
	auto pCell = MapClass::Instance.TryGetCellAt(coords);

	const auto isClear = [pPayloadType](CellClass* pCell)
		{
			return pCell && pCell->IsClearToMove(pPayloadType->SpeedType, true, true, -1,
				pPayloadType->MovementZone, pCell->GetLevel(), pCell->ContainsBridge());
		};

	// The cell right below is the natural drop spot; look for the closest usable one if it is taken.
	if (!isClear(pCell))
	{
		const auto freeCell = MapClass::Instance.NearByLocation(CellClass::Coord2Cell(coords),
			pPayloadType->SpeedType, -1, pPayloadType->MovementZone, false, 1, 1, false, false, false,
			true, CellStruct::Empty, false, false);

		if (freeCell != CellStruct::Empty)
		{
			if (auto const pFreeCell = MapClass::Instance.TryGetCellAt(freeCell))
			{
				const auto freeCoords = pFreeCell->GetCoords();
				coords.X = freeCoords.X;
				coords.Y = freeCoords.Y;
				pCell = pFreeCell;
			}
		}
	}

	const bool onBridge = pCell && pCell->ContainsBridge();
	const int floorHeight = MapClass::Instance.GetCellFloorHeight(coords) + (onBridge ? CellClass::BridgeHeight : 0);

	// Release the cargo where the carrier is and let it come down by itself.
	coords.Z = Math::max(coords.Z, floorHeight);
	pPayload->OnBridge = onBridge;

	const auto facing = pPayload->PrimaryFacing.Current().GetDir();

	if (!pPayload->Unlimbo(coords, facing))
	{
		// Nowhere to put it down - keep it slung instead of destroying it.
		pPayload->OnBridge = false;
		this->JumpjetCarryall_Payload = pPayload;
		pPayloadExt->JumpjetCarryall_Carrier = pThis;
		pPayload->IsOnCarryall = true;
		return false;
	}

	if (pPayload->IsInAir())
	{
		if (auto const pPayloadLoco = locomotion_cast<JumpjetLocomotionClass*>(pPayload->Locomotor))
		{
			// A jumpjet flies itself down.
			if (pPayloadType->BalloonHover)
			{
				pPayloadLoco->State = JumpjetLocomotionClass::State::Hovering;
				pPayloadLoco->IsMoving = true;
				pPayloadLoco->DestinationCoords = pPayload->Location;
			}
			else
			{
				pPayloadLoco->Move_To(pPayload->Location);
			}
		}
		else
		{
			// Everything else falls, the same way a unit does when it loses a flying locomotor.
			pPayload->IsFallingDown = true;
			TechnoExt::Fetch(pPayload)->OnParachuted = true;

			if (pPayload->WhatAmI() == AbstractType::Infantry)
				static_cast<InfantryClass*>(pPayload)->PlayAnim(Sequence::Paradrop, true, false);
		}
	}

	pPayload->Mark(MarkType::Change);

	const int sound = this->GetTypeExtData()->JumpjetCarryall_DropoffSound.Get(pThis->Type->LeaveTransportSound);

	if (sound != -1)
		VocClass::PlayAt(sound, coords);

	return true;
}

void UnitExt::ReleaseJumpjetCarryallPayloadOnDeath()
{
	auto const pPayload = this->JumpjetCarryall_Payload;

	if (!pPayload)
		return;

	if (this->GetTypeExtData()->JumpjetCarryall_ReleaseOnDeath && this->DropJumpjetCarryallPayload())
		return;

	// Either the carrier takes its cargo with it, or there was nowhere to set it down.
	this->JumpjetCarryall_Payload = nullptr;

	if (auto const pPayloadExt = FootExt::TryFetch(pPayload))
		pPayloadExt->JumpjetCarryall_Carrier = nullptr;

	pPayload->IsOnCarryall = false;

	if (pPayload->IsAlive)
	{
		pPayload->RegisterDestruction(this->OwnerObject());
		pPayload->UnInit();
	}
}

void UnitExt::OnJumpjetCarryallDetach(FootClass* pTarget)
{
	if (this->JumpjetCarryall_Payload == pTarget)
		this->JumpjetCarryall_Payload = nullptr;

	if (this->JumpjetCarryall_Target == pTarget)
	{
		this->JumpjetCarryall_Target = nullptr;

		if (this->JumpjetCarryall_State > JumpjetCarryallState::Ready)
			this->CancelJumpjetCarryallMission(true);
	}
}
