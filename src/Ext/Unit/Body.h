#pragma once

#include <Ext/Foot/Body.h>
#include <Ext/UnitType/Body.h>
#include <UnitClass.h>

// Phase of a jumpjet carryall pickup mission.
enum class JumpjetCarryallState : int
{
	Inactive = 0, // Idle on the ground, or carrying cargo with no mission running.
	Ready = 1,    // Idle in the air with a free sling.
	Approach = 2, // Flying towards the target's cell.
	Descend = 3,  // Over the target, lowering onto it.
	Ascend = 4,   // Cargo secured, climbing back to cruise height.
};

// Concrete leaf extension for UnitClass. Empty for now: all techno-level data lives
// in TechnoExt; this leaf only exists so a unit's extension has its own
// concrete type (TechnoExt itself is never instantiated).
class UnitExt final : public FootExt
{
public:
	using base_type = UnitClass;

	static constexpr DWORD Canary = 0xE1E2E3E4;

	int SubterraneanHarvStatus; // 0 = none, 1 = created, 2 = out from factory
	AbstractClass* SubterraneanHarvRallyPoint;
	bool ReceiveDamage;
	CDTimerClass DeployFireTimer;
	bool KeepTargetOnMove;
	CDTimerClass SimpleDeployerAnimationTimer;
	bool IsBurrowed;
	bool UndergroundTracked;

	std::vector<RecoilData> ExtraTurretRecoil;
	std::vector<RecoilData> ExtraBarrelRecoil;

	FootClass* JumpjetCarryall_Payload; // Cargo currently slung under this carrier.
	FootClass* JumpjetCarryall_Target; // Pickup target of the running carryall mission.
	CellStruct JumpjetCarryall_TargetCell; // Last known cell of the pickup target.
	JumpjetCarryallState JumpjetCarryall_State;
	int JumpjetCarryall_Timer; // Frames the running pickup, or the climb after one, has spent without progress.
	int JumpjetCarryall_BestDistance; // Closest the carrier has ever been to the pickup target, in cells.

	explicit UnitExt(UnitClass* const OwnerObject) : FootExt(OwnerObject)
		, SubterraneanHarvStatus { 0 }
		, SubterraneanHarvRallyPoint { nullptr }
		, ReceiveDamage { false }
		, DeployFireTimer {}
		, KeepTargetOnMove { false }
		, SimpleDeployerAnimationTimer {}
		, IsBurrowed { false }
		, UndergroundTracked { false }
		, ExtraTurretRecoil {}
		, ExtraBarrelRecoil {}
		, JumpjetCarryall_Payload { nullptr }
		, JumpjetCarryall_Target { nullptr }
		, JumpjetCarryall_TargetCell {}
		, JumpjetCarryall_State { JumpjetCarryallState::Inactive }
		, JumpjetCarryall_Timer { 0 }
		, JumpjetCarryall_BestDistance { INT_MAX }
	{ }

	virtual ~UnitExt() override;

	virtual bool IsBurrowedState() const override { return this->IsBurrowed; }

	void UpdateSubterraneanHarvester();
	void UpdateKeepTargetOnMove();
	void DepletedAmmoActions();
	void InitializeRecoilData();
	void UpdateRecoilData();
	void RecordRecoilData();

	// Jumpjet carryall
	bool IsJumpjetCarryall() const;

	// True only while the carrier is over its pickup target and lowering onto it. The
	// target's cell is occupied by definition, so the locomotor's landing pathfinding
	// checks have to be waived for the carrier to reach it.
	bool IsJumpjetCarryallLandingOnTarget() const;

	double GetJumpjetCarryallSpeedMultiplier() const;
	bool CanLiftJumpjetCargo(TechnoClass* pTarget) const;

	// Advances the pickup's watchdog. Returns false when the mission has to be given up,
	// either because the player called the carrier off or because it has spent too long
	// getting no closer to the target.
	bool TickJumpjetCarryallPickup();

	void StartJumpjetCarryallMission(FootClass* pTarget);
	void CancelJumpjetCarryallMission(bool keepReady = false);
	void UpdateJumpjetCarryall();
	bool DropJumpjetCarryallPayload();
	void ReleaseJumpjetCarryallPayloadOnDeath();
	void OnJumpjetCarryallDetach(FootClass* pTarget);

	static UnitClass* Deployer;

	static bool CannotMove(UnitClass* pThis);
	static bool HasAmmoToDeploy(UnitClass* pThis);
	static void HandleOnDeployAmmoChange(UnitClass* pThis, int maxAmmoOverride = -1);
	static bool SimpleDeployerAllowedToDeploy(UnitClass* pThis, bool defaultValue, bool alwaysCheckLandTypes);
	static bool CanDeployIntoBuilding(UnitClass* pThis, bool noDeploysIntoDefaultValue = false);
	static UnitTypeClass* GetUnitTypeExtra(UnitClass* pUnit, UnitTypeExt* pData);

	UnitClass* OwnerObject() const
	{
		return static_cast<UnitClass*>(this->GetAttachedObject());
	}

	// a unit's type extension is always the UnitTypeExt leaf
	UnitTypeExt* GetTypeExtData() const
	{
		return static_cast<UnitTypeExt*>(this->TypeExtData);
	}

	class ExtContainer final : public Container<UnitExt>
	{
	public:
		ExtContainer();
		~ExtContainer();
	};

	static ExtContainer ExtMap;

	static UnitExt* Fetch(const UnitClass* pThis)
	{
		return AbstractExt::Fetch<UnitExt>(pThis);
	}

	static UnitExt* TryFetch(const UnitClass* pThis)
	{
		return AbstractExt::TryFetch<UnitExt>(pThis);
	}

	virtual void LoadFromStream(PhobosStreamReader& Stm) override;
	virtual void SaveToStream(PhobosStreamWriter& Stm) override;

private:
	template <typename T>
	void Serialize(T& Stm);
};
