#include <Stratega/Agent/CombatAgent.h>

namespace SGA
{
	std::vector<Action> CombatAgent::filterUnitActions(std::vector<Action>& actions, Entity& unit) const
	{
		std::vector<Action> filteredActions;
		for (const auto& a : actions)
		{
			if (a.actionTypeID != -1 && a.isEntityAction() && a.targets[0].getEntityID() == unit.id)
			{
				filteredActions.emplace_back(a);
			}
		}
		return filteredActions;
	}

	std::vector<Action> CombatAgent::filterActionTypes(std::vector<Action>& actions, std::string type) const
	{
		std::vector<Action> filteredActions;

		for (const auto& a : actions)
		{
			if (actionTypeIDToActionTypeString.at(a.actionTypeID) == type)
			{
				filteredActions.emplace_back(a);
			}
		}
		return filteredActions;
	}

	std::vector<Entity*> CombatAgent::filterUnitsByReach(const std::vector<Entity*>& targetUnits, const Vector2f& pos, GameState& currentState) const
	{
		std::vector<Entity*> units;
		
		for (const auto& unit : targetUnits)
		{
			if (pos.manhattanDistance(unit->position) <= getAttackRange(unit, currentState) + getMovementRange(unit, currentState));
			{
				units.emplace_back(unit);
			}
		}
		
		return units;
	}

	std::vector<Action> CombatAgent::filterMovesInRange(const std::vector<SGA::Action>& moves, Vector2f position, int range, GameState& gameState) const
	{
		std::vector<Action> filteredMoves;
		
		for (const auto& move : moves)
		{
			if (move.targets[1].getPosition(gameState).manhattanDistance(position) <= range)
				filteredMoves.emplace_back(move);
		}
		
		return filteredMoves;
	}

	int CombatAgent::getPotentialHealing(std::vector<Action>& actions,  const Vector2f& pos, const std::vector<Entity*>& potentialHealers, GameState& gameState) const
	{
		auto healers = filterUnitsByReach(potentialHealers, pos, gameState);
		auto heal = 0;

		auto healingActions = filterActionTypes(actions, "Heal");
		
		for (const auto* healer : healers)
		{
			// Check if the unit can heal
			for (auto const& healingAction : healingActions)
			{
				if (healingAction.targets[0].getEntityID() == healer->id)
				{
					heal += getHealAmount(healer, gameState);
					break;
				}
			}

		}
		
		return heal;
	}

	bool CombatAgent::getMoveInRange(Entity& u, const Vector2f& pos, int range, const std::vector<Entity*>& opponentUnits, std::vector<SGA::Action>& moves, Action& bucket, GameState& gameState) const
	{		
		if (u.position.manhattanDistance(pos) <= range)
			return false;

		auto inRangeMoves = filterMovesInRange(moves, pos, range, gameState);
		bool foundAction = false;
		if (inRangeMoves.empty())
		{
			// Move closer to the target position, while keeping a decent strategic position
			double bestScore = std::numeric_limits<double>::lowest();
			// Choose the best strategic position
			for (const auto& move : moves)
			{
				
				auto dist = pos.manhattanDistance(move.targets[1].getPosition(gameState));
				auto score = -dist;
				if (score > bestScore)
				{
					bestScore = score;
					bucket = move;
					foundAction = true;
				}
			}
		}
		else
		{
			double bestScore = std::numeric_limits<double>::lowest();
			// Choose the best strategic position
			for (const auto& move : inRangeMoves)
			{
				auto score = -getPotentialDamage(move.targets[1].getPosition(gameState), opponentUnits, gameState);
				if (score > bestScore)
				{
					bestScore = score;
					bucket = move;
					foundAction = true;
				}
			}
		}

		return foundAction;
		
	}

	double CombatAgent::getPotentialDamage(const Vector2f& pos, const std::vector<Entity*>& potentialAttackers, GameState& gameState) const
	{
		
		auto attackers = filterUnitsByReach(potentialAttackers, pos, gameState);
		double damage = 0;
		
		for (const auto* attacker : attackers)
		{
			damage += getAttackDamage(attacker, gameState);
		}
		
		return damage;
	}

	double CombatAgent::getAttackScore(std::vector<Action>& actions, const Entity& target, const Action& attack, const std::vector<Entity*>& opponentUnits, GameState& gameState) const
	{
		
		auto attackAmount = getAttackDamage(attack.targets[0].getEntity(gameState), gameState);
		auto healAmount = getPotentialHealing(actions, target.position, opponentUnits, gameState);

		if (attackAmount >= getHealth(&target, gameState))
		{
			// We can kill the unit immediatly
			return unitScores.at(target.typeID) * 2;
		}
		else if (healAmount < attackAmount)
		{
			// We can kill the unit with an delay
			int turnsToKill = std::min(4., std::ceil(getHealth(&target, gameState)/ (attackAmount - healAmount)));
			return unitScores.at(target.typeID) * (1. + 1. / (1. + turnsToKill));
		}

		// We can't kill the unit alone
		//return unitScores.at(target.typeID) * 0.5;
		return 0;
	}

	double CombatAgent::getHealScore(std::vector<Action>& actions, const Entity& target, const Action& heal, const std::vector<Entity*>& opponentUnits, GameState& gameState) const
	{
		auto healAmount = getHealAmount(heal.targets[0].getEntity(gameState), gameState);
		auto resultingHealth = std::min<double> (getMaxHealth(&target, gameState), getHealth(&target, gameState) + healAmount);
		auto potentialDamage = getPotentialDamage(target.position, opponentUnits, gameState);

		if (potentialDamage < 0.0001)
		{
			// The unit is in no immediate danger
			//return unitScores.at(target.typeID) * 0.5 * (resultingHealth - getHealth(&target, gameState)) / static_cast<double>(healAmount);
		}
		else if (potentialDamage > resultingHealth)
		{
			// The unit will die if the opponent wants it
			//return unitScores.at(target.typeID) * 0.2 * (resultingHealth - getHealth(&target, gameState)) / static_cast<double>(healAmount);
		}
		else if (healAmount >= potentialDamage)
		{
			// We can keep the unit alive forever
			return 2 * unitScores.at(target.typeID);
		}
		else
		{
			// We can delay the death
			int turnsUntilDeath = std::min<int> (4, static_cast<int>(std::ceil(getHealth(&target, gameState) / (potentialDamage - healAmount))));
			return (1. + turnsUntilDeath / 4.) * unitScores.at(target.typeID);
		}
		return 0;
	}

	double CombatAgent::getMovementRange(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "MovementPoints")
			{
				return entity->parameters[param.second.index];
			}
		}
	}
	
	double CombatAgent::getHealth(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "Health")
			{
				return entity->parameters[param.second.index];
			}
		}
	}

	double CombatAgent::getMaxHealth(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "Health")
			{
				return param.second.maxValue;
			}
		}
		return 0;
	}

	double CombatAgent::getAttackRange(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "AttackRange")
			{
				return entity->parameters[param.second.index];
			}
		}
		return 0;
	}

	double CombatAgent::getAttackDamage(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "AttackDamage")
			{
				return entity->parameters[param.second.index];
			}
		}
		return 0;
	}

	double CombatAgent::getHealAmount(const Entity* entity, GameState& gamestate) const
	{
		for (const auto& param : (*gamestate.gameInfo->entityTypes)[entity->id].parameters)
		{
			if (param.second.name == "AttackDamage")
			{
				return entity->parameters[param.second.index];
			}
		}
		return 0;
	}
	
	ActionAssignment CombatAgent::playTurn(GameState& currentState, const EntityForwardModel& fm)
	{
		for (const auto a : *currentState.gameInfo->actionTypes)
		{
			actionTypeIDToActionTypeString[a.first] = a.second.name;
		}
		actionTypeIDToActionTypeString[-1] = "EndTurn";
			
			

		//std::cout << "CombatAgent " << currentState.currentGameTurn << std::endl;

		// Get my units and opponent units
		std::vector<Entity*> myUnits;
		std::vector<Entity*> opponentUnits;
		for (auto& entity : currentState.entities) {
			if (entity.ownerID == getPlayerID())
			{
				myUnits.push_back(&entity);
			}
			else
			{
				opponentUnits.push_back(&entity);
			}
				
		}
			
			
			
		// Compute the best target that we should attack, based on how much support it has and how easy we can attack it
		Entity* bestAttackTarget = nullptr;
		double highestScore = std::numeric_limits<double>::lowest();
		for (auto* opp : opponentUnits)
		{
				
			// How much support has the unit? Computed by estimating how long it reaches for support to arrive and how strong it is.
			double avgSupportScore = 0;
				
			for (auto* ally : opponentUnits)
			{
				if (ally->id == opp->id)
					continue;

				int dist = opp->position.manhattanDistance(ally->position);
				int movesToSupport = dist / static_cast<double>(getMovementRange(ally, currentState));
				avgSupportScore += unitScores.at(ally->typeID) / (1. + movesToSupport);
			}
			avgSupportScore /= opponentUnits.size();

				
			// How much attack power do we have? Computed by estimating how long it takes to attack and how strong our units are.
			double avgAttackScore = 0;
			for (auto* attacker : myUnits)
			{
				int dist = opp->position.chebyshevDistance(attacker->position);
				int movesToAttack = std::max(0, dist - static_cast<int>(getMovementRange(attacker, currentState))) / getMovementRange(attacker, currentState);
				avgAttackScore += unitScores.at(attacker->typeID) / (1. + movesToAttack);
			}
			avgAttackScore /= myUnits.size() + 1;

			// Is this a better target than a previously found target?
			double score = avgAttackScore - avgSupportScore;
			if (score > highestScore)
			{
				highestScore = score;
				bestAttackTarget = opp;
			}
				
		}
			
			
		Vector2f moveTarget;
		// We found no enemy, so we move to a random position in order to find one
		if (bestAttackTarget == nullptr)
		{
			auto& rngEngine = getRNGEngine();
			std::uniform_int_distribution<int> widthDist(0, currentState.board.getWidth() - 1);
			std::uniform_int_distribution<int> heightDist(0, currentState.board.getHeight() - 1);
			moveTarget.x = widthDist(rngEngine);
			moveTarget.y = heightDist(rngEngine);
		}
		else
		{
			moveTarget = bestAttackTarget->position;
		}

		auto actions = fm.generateActions(currentState, getPlayerID());
		Action nextAction = filterActionTypes(actions, "EndTurn").at(0); // Only one EndTurn action available
		bool foundAction = false;
		for (auto* unit : myUnits)
		{
			auto subActions = filterUnitActions(actions, *unit);
			// First try attacking something
			highestScore = std::numeric_limits<double>::lowest();
			for (const auto& attack : filterActionTypes(subActions, "Attack"))
			{
				Entity& targetUnit = *attack.targets[1].getEntity(currentState);
				if (targetUnit.ownerID == getPlayerID())
					continue; // No attackerino my own units

				auto score = getAttackScore(actions, targetUnit, attack, opponentUnits, currentState);
				if (score > highestScore)
				{
					highestScore = score;
					nextAction = attack;
					foundAction = true;
				}
			}

			if (foundAction)
				break;

			// Try healing something
			highestScore = std::numeric_limits<double>::lowest();
			for (const auto& heal : filterActionTypes(subActions, "Heal"))
			{
				Entity& targetUnit = *heal.targets[0].getEntity(currentState);
				if (targetUnit.ownerID != getPlayerID())
					continue; // No healerino opponents units
				if (getHealth(&targetUnit, currentState) >= getMaxHealth(&targetUnit, currentState))
					continue; // Stop healing units that are already full, what is wrong with you

				auto score = getHealScore(actions, targetUnit, heal, opponentUnits, currentState);
				if (score > highestScore)
				{
					highestScore = score;
					nextAction = heal;
					foundAction = true;
				}
			}

			if (foundAction)
				break;

			// At last, try moving closer to the best attack target
			auto moves = filterActionTypes(subActions, "Move");
			if (getMoveInRange(*unit, moveTarget, getAttackRange(unit, currentState), opponentUnits, moves, nextAction, currentState))
			{
				break;
			}
		}
		if (nextAction.actionTypeFlags == ActionFlag::EndTickAction)
		{
			std::cout << "Combat Agent " << "end round" << std::endl;
		} else
		{
			std::cout << "Combat Agent " << "does something" << std::endl;
		}
			
		return ActionAssignment::fromSingleAction(nextAction);		
	}

	ActionAssignment CombatAgent::computeAction(GameState state, const EntityForwardModel& forwardModel, long timeBudgetMs)
	{
		unitScores = UnitTypeEvaluator::getLinearSumEvaluation(state);
		return playTurn(state, forwardModel);
	}

}