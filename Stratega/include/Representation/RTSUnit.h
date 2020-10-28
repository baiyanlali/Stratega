#pragma once
#include <Representation/Vector2.h>
#include <Representation/RTSGameState.h>
#include <ForwardModel/ActionType.h>
#include <ForwardModel/Action.h>
#include <Representation/Path.h>

namespace SGA
{
	class RTSGameState;

	class RTSUnit
	{
	public:
		RTSUnit(RTSGameState& state, int unitId, int playerId) :
			playerID(playerId),
			unitID(unitId),
			unitTypeID(0),
			actionRange(0),
			actionCooldown(0),
			maxActionCooldown(1),
			movementSpeed(2),
			collisionRadius(0.5),
			position(),
			health(40),
			maxHealth(health),
			attackDamage(20),
			healAmount(10),
			state(state),
			intendedAction(),
			executingAction(),
			path()
		{
		}

		int playerID;
		int unitID;
		int unitTypeID;

		double actionRange;
		double actionCooldown;
		double maxActionCooldown;
		double movementSpeed;
		float collisionRadius;
		
		Vector2f position;
		int health;
		int maxHealth;
		int attackDamage;
		int healAmount;
		std::reference_wrapper<RTSGameState> state;
		
		// Actions
		Action<Vector2f> intendedAction;
		Action<Vector2f> executingAction;
		Path path;
	};
}
