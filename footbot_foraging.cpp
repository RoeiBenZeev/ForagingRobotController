/* Include the controller definition */
#include "footbot_foraging.h"
/* Function definitions for XML parsing */
#include <argos3/core/utility/configuration/argos_configuration.h>
/* 2D vector definition */
#include <argos3/core/utility/math/vector2.h>
/* Logging */
#include <argos3/core/utility/logging/argos_log.h>

/****************************************/
/****************************************/

CFootBotForaging::SFoodData::SFoodData() :
   HasFoodItem(false),
   FoodItemIdx(0),
   TotalFoodItems(0) {}

void CFootBotForaging::SFoodData::Reset() {
   HasFoodItem = false;
   FoodItemIdx = 0;
   TotalFoodItems = 0;
}

/****************************************/
/****************************************/

CFootBotForaging::SDiffusionParams::SDiffusionParams() :
   GoStraightAngleRange(CRadians(-1.0f), CRadians(1.0f)) {}

void CFootBotForaging::SDiffusionParams::Init(TConfigurationNode& t_node) {
   try {
      CRange<CDegrees> cGoStraightAngleRangeDegrees(CDegrees(-10.0f), CDegrees(10.0f));
      GetNodeAttribute(t_node, "go_straight_angle_range", cGoStraightAngleRangeDegrees);
      GoStraightAngleRange.Set(ToRadians(cGoStraightAngleRangeDegrees.GetMin()),
                               ToRadians(cGoStraightAngleRangeDegrees.GetMax()));
      GetNodeAttribute(t_node, "delta", Delta);
   }
   catch(CARGoSException& ex) {
      THROW_ARGOSEXCEPTION_NESTED("Error initializing controller diffusion parameters.", ex);
   }
}

/****************************************/
/****************************************/

void CFootBotForaging::SWheelTurningParams::Init(TConfigurationNode& t_node) {
   try {
      TurningMechanism = NO_TURN;
      CDegrees cAngle;
      GetNodeAttribute(t_node, "hard_turn_angle_threshold", cAngle);
      HardTurnOnAngleThreshold = ToRadians(cAngle);
      GetNodeAttribute(t_node, "soft_turn_angle_threshold", cAngle);
      SoftTurnOnAngleThreshold = ToRadians(cAngle);
      GetNodeAttribute(t_node, "no_turn_angle_threshold", cAngle);
      NoTurnAngleThreshold = ToRadians(cAngle);
      GetNodeAttribute(t_node, "max_speed", MaxSpeed);
   }
   catch(CARGoSException& ex) {
      THROW_ARGOSEXCEPTION_NESTED("Error initializing controller wheel turning parameters.", ex);
   }
}

/****************************************/
/****************************************/

CFootBotForaging::SStateData::SStateData() :
   ProbRange(0.0f, 1.0f) {}

void CFootBotForaging::SStateData::Init(TConfigurationNode& t_node) {
   try {
      GetNodeAttribute(t_node, "initial_rest_to_explore_prob", InitialRestToExploreProb);
      GetNodeAttribute(t_node, "initial_explore_to_rest_prob", InitialExploreToRestProb);
      GetNodeAttribute(t_node, "food_rule_explore_to_rest_delta_prob", FoodRuleExploreToRestDeltaProb);
      GetNodeAttribute(t_node, "food_rule_rest_to_explore_delta_prob", FoodRuleRestToExploreDeltaProb);
      GetNodeAttribute(t_node, "collision_rule_explore_to_rest_delta_prob", CollisionRuleExploreToRestDeltaProb);
      GetNodeAttribute(t_node, "social_rule_rest_to_explore_delta_prob", SocialRuleRestToExploreDeltaProb);
      GetNodeAttribute(t_node, "social_rule_explore_to_rest_delta_prob", SocialRuleExploreToRestDeltaProb);
      GetNodeAttribute(t_node, "minimum_resting_time", MinimumRestingTime);
      GetNodeAttribute(t_node, "minimum_unsuccessful_explore_time", MinimumUnsuccessfulExploreTime);
      GetNodeAttribute(t_node, "minimum_search_for_place_in_nest_time", MinimumSearchForPlaceInNestTime);

   }
   catch(CARGoSException& ex) {
      THROW_ARGOSEXCEPTION_NESTED("Error initializing controller state parameters.", ex);
   }
}

void CFootBotForaging::SStateData::Reset() {
   State = STATE_RESTING;
   InNest = true;
   RestToExploreProb = InitialRestToExploreProb;
   ExploreToRestProb = InitialExploreToRestProb;
   TimeExploringUnsuccessfully = 0;
   /* Initially the robot is resting, and by setting RestingTime to
      MinimumRestingTime we force the robots to make a decision at the
      experiment start. If instead we set RestingTime to zero, we would
      have to wait till RestingTime reaches MinimumRestingTime before
      something happens, which is just a waste of time. */
   TimeRested = MinimumRestingTime;
   TimeSearchingForPlaceInNest = 0;
}

/****************************************/
/****************************************/

CFootBotForaging::CFootBotForaging() :
   m_pcWheels(NULL),
   m_pcLEDs(NULL),
   m_pcRABA(NULL),
   m_pcRABS(NULL),
   m_pcProximity(NULL),
   m_pcLight(NULL),
   m_pcGround(NULL),
   m_pcRNG(NULL) {}

/****************************************/
/****************************************/

void CFootBotForaging::Init(TConfigurationNode& t_node) {
   try {
      /*
       * Initialize sensors/actuators
       */
      m_pcWheels    = GetActuator<CCI_DifferentialSteeringActuator>("differential_steering");
      m_pcLEDs      = GetActuator<CCI_LEDsActuator                >("leds"                 );
      m_pcRABA      = GetActuator<CCI_RangeAndBearingActuator     >("range_and_bearing"    );
      m_pcRABS      = GetSensor  <CCI_RangeAndBearingSensor       >("range_and_bearing"    );
      m_pcProximity = GetSensor  <CCI_FootBotProximitySensor      >("footbot_proximity"    );
      m_pcLight     = GetSensor  <CCI_FootBotLightSensor          >("footbot_light"        );
      m_pcGround    = GetSensor  <CCI_FootBotMotorGroundSensor    >("footbot_motor_ground" );
      m_OmniCamera  = GetSensor  <CCI_ColoredBlobOmnidirectionalCameraSensor>("colored_blob_omnidirectional_camera");
       //Enable camera when we are in collision
       m_OmniCamera->Enable();
      /*
       * Parse XML parameters
       */
      /* Diffusion algorithm */
      m_sDiffusionParams.Init(GetNode(t_node, "diffusion"));
      /* Wheel turning */
      m_sWheelTurningParams.Init(GetNode(t_node, "wheel_turning"));
      /* Controller state */
      m_sStateData.Init(GetNode(t_node, "state"));
      /* Collision Q learning*/
      m_sCollision.Init();

      //m_OmniCamera->Init(t_node);

   }
   catch(CARGoSException& ex) {
      THROW_ARGOSEXCEPTION_NESTED("Error initializing the foot-bot foraging controller for robot \"" << GetId() << "\"", ex);
   }
   /*
    * Initialize other stuff
    */
   /* Create a random number generator. We use the 'argos' category so
      that creation, reset, seeding and cleanup are managed by ARGoS. */
   m_pcRNG = CRandom::CreateRNG("argos");
   Reset();
}

/****************************************/
/****************************************/

void CFootBotForaging::ControlStep() {
    switch(m_sStateData.State) {
       case SStateData::STATE_RESTING: {
         dropFood();
          break;
       }
       case SStateData::STATE_EXPLORING: {
          //Explore();
          findPuck();
          break;
       }
       case SStateData::STATE_RETURN_TO_NEST: {
          //ReturnToNest();
          goHome();
          break;
       }
       case SStateData::STATE_LEAVE_HOME: {
           leaveHome();
           break;
       }
       default: {
          LOGERR << "We can't be here, there's a bug!" << std::endl;
       }
    }
}

/****************************************/
/****************************************/

void CFootBotForaging::Reset() {
   /* Reset robot state */
   m_sStateData.Reset();
   /* Reset food data */
   m_sFoodData.Reset();
   /* Set LED color */
   m_pcLEDs->SetAllColors(CColor::RED);
   /* Clear up the last exploration result */
   m_eLastExplorationResult = LAST_EXPLORATION_NONE;
   m_pcRABA->ClearData();
   m_pcRABA->SetData(0, LAST_EXPLORATION_NONE);
}

/****************************************/
/****************************************/

void CFootBotForaging::UpdateState() {
   /* Reset state flags */
   m_sStateData.InNest = false;
   /* Read stuff from the ground sensor */
   const CCI_FootBotMotorGroundSensor::TReadings& tGroundReads = m_pcGround->GetReadings();
   /*
    * You can say whether you are in the nest by checking the ground sensor
    * placed close to the wheel motors. It returns a value between 0 and 1.
    * It is 1 when the robot is on a white area, it is 0 when the robot
    * is on a black area and it is around 0.5 when the robot is on a gray
    * area.
    * The foot-bot has 4 sensors like this, two in the front
    * (corresponding to readings 0 and 1) and two in the back
    * (corresponding to reading 2 and 3).  Here we want the back sensors
    * (readings 2 and 3) to tell us whether we are on gray: if so, the
    * robot is completely in the nest, otherwise it's outside.
    */
   if(tGroundReads[2].Value > 0.25f &&
      tGroundReads[2].Value < 0.75f &&
      tGroundReads[3].Value > 0.25f &&
      tGroundReads[3].Value < 0.75f) {
      m_sStateData.InNest = true;
   }
}

/****************************************/
/****************************************/

CVector2 CFootBotForaging::CalculateVectorToLight() {
   /* Get readings from light sensor */
   const CCI_FootBotLightSensor::TReadings& tLightReads = m_pcLight->GetReadings();
   /* Sum them together */
   CVector2 cAccumulator;

   for(size_t i = 0; i < tLightReads.size(); ++i) {
      cAccumulator += CVector2(tLightReads[i].Value, tLightReads[i].Angle);
   }
   /* If the light was perceived, return the vector */
   if(cAccumulator.Length() > 0.0f) {
      return CVector2(1.0f, cAccumulator.Angle());
   }
   /* Otherwise, return zero */
   else {
      return CVector2();
   }
}

/****************************************/
/****************************************/

CVector2 CFootBotForaging::DiffusionVector(bool& b_collision) {
   /* Get readings from proximity sensor */
   const CCI_FootBotProximitySensor::TReadings& tProxReads = m_pcProximity->GetReadings();


   /*Check id this a robot collision*/

   /* Sum them together */
   CVector2 cDiffusionVector;
   for(size_t i = 0; i < tProxReads.size(); ++i) {
      cDiffusionVector += CVector2(tProxReads[i].Value, tProxReads[i].Angle);
   }
   /* If the angle of the vector is small enough and the closest obstacle
      is far enough, ignore the vector and go straight, otherwise return
      it */
   if(m_sDiffusionParams.GoStraightAngleRange.WithinMinBoundIncludedMaxBoundIncluded(cDiffusionVector.Angle()) &&
      cDiffusionVector.Length() < m_sDiffusionParams.Delta ) {
      b_collision = false;

       return CVector2::X;
   }
   else {
       /*Means we are in collision*/
      b_collision = true;

       const CCI_ColoredBlobOmnidirectionalCameraSensor::SReadings& sOmniReads = m_OmniCamera->GetReadings();
       bool b_interRobotCollision = false;

       for(int i = 0 ; i < sOmniReads.BlobList.size();i++) {
           if(!m_sDiffusionParams.GoStraightAngleRange.WithinMinBoundIncludedMaxBoundIncluded(cDiffusionVector.Angle())) {
               CRadians Angle =  cDiffusionVector.Angle() - sOmniReads.BlobList[i]->Angle;
               //Check if any if the omni camera reads is in the collision vector angle range.
               //The Angle var cant be 0 most of the time even if that a robot collision so we take a smaller enough value.
                if (Angle.GetAbsoluteValue() < 0.25) {
                    /*If we got here , its  a inter robots collision with high probability*/
                    b_interRobotCollision = true;
                    //argos::LOG << Angle.GetAbsoluteValue() << std::endl;
                }
           }
       }
       if (b_interRobotCollision) {
           /*Here we define our inter robots collision handeler*/
           //argos::LOG << "inter collision" << std::endl;
           return vecGoRight(cDiffusionVector).Normalize();
       }

       //argos::LOG << cDiffusionVector << std::endl;

       cDiffusionVector.Normalize();
       //argos::LOG << cDiffusionVector << std::endl;

       return -cDiffusionVector;
   }
}

CVector2 CFootBotForaging::vecGoRight(CVector2 myVec){
    CVector2 cDiffusionVector = CVector2::X;
    CRadians rightAngle(CRadians().PI + CRadians().PI_OVER_TWO);

    argos::LOG << myVec << std::endl;
    myVec.Rotate(rightAngle);
    argos::LOG << myVec << std::endl;

    return myVec;
}

CVector2 CFootBotForaging::vecGoBack(CVector2 myVec) {
    CVector2 cDiffusionVector = CVector2::X;
    CRadians rightAngle(CRadians().PI);

    argos::LOG << myVec << std::endl;
    myVec.Rotate(rightAngle);
    argos::LOG << myVec << std::endl;

    return myVec;
}

/****************************************/
/****************************************/

void CFootBotForaging::SetWheelSpeedsFromVector(const CVector2& c_heading) {
   /* Get the heading angle */
   CRadians cHeadingAngle = c_heading.Angle().SignedNormalize();
   /* Get the length of the heading vector */
   Real fHeadingLength = c_heading.Length();
   /* Clamp the speed so that it's not greater than MaxSpeed */
   Real fBaseAngularWheelSpeed = Min<Real>(fHeadingLength, m_sWheelTurningParams.MaxSpeed);
   /* State transition logic */
   if(m_sWheelTurningParams.TurningMechanism == SWheelTurningParams::HARD_TURN) {
      if(Abs(cHeadingAngle) <= m_sWheelTurningParams.SoftTurnOnAngleThreshold) {
         m_sWheelTurningParams.TurningMechanism = SWheelTurningParams::SOFT_TURN;
      }
   }
   if(m_sWheelTurningParams.TurningMechanism == SWheelTurningParams::SOFT_TURN) {
      if(Abs(cHeadingAngle) > m_sWheelTurningParams.HardTurnOnAngleThreshold) {
         m_sWheelTurningParams.TurningMechanism = SWheelTurningParams::HARD_TURN;
      }
      else if(Abs(cHeadingAngle) <= m_sWheelTurningParams.NoTurnAngleThreshold) {
         m_sWheelTurningParams.TurningMechanism = SWheelTurningParams::NO_TURN;
      }
   }
   if(m_sWheelTurningParams.TurningMechanism == SWheelTurningParams::NO_TURN) {
      if(Abs(cHeadingAngle) > m_sWheelTurningParams.HardTurnOnAngleThreshold) {
         m_sWheelTurningParams.TurningMechanism = SWheelTurningParams::HARD_TURN;
      }
      else if(Abs(cHeadingAngle) > m_sWheelTurningParams.NoTurnAngleThreshold) {
         m_sWheelTurningParams.TurningMechanism = SWheelTurningParams::SOFT_TURN;
      }
   }
   /* Wheel speeds based on current turning state */
   Real fSpeed1, fSpeed2;
   switch(m_sWheelTurningParams.TurningMechanism) {
      case SWheelTurningParams::NO_TURN: {
         /* Just go straight */
         fSpeed1 = fBaseAngularWheelSpeed;
         fSpeed2 = fBaseAngularWheelSpeed;
         break;
      }
      case SWheelTurningParams::SOFT_TURN: {
         /* Both wheels go straight, but one is faster than the other */
         Real fSpeedFactor = (m_sWheelTurningParams.HardTurnOnAngleThreshold - Abs(cHeadingAngle)) / m_sWheelTurningParams.HardTurnOnAngleThreshold;
         fSpeed1 = fBaseAngularWheelSpeed - fBaseAngularWheelSpeed * (1.0 - fSpeedFactor);
         fSpeed2 = fBaseAngularWheelSpeed + fBaseAngularWheelSpeed * (1.0 - fSpeedFactor);
         break;
      }
      case SWheelTurningParams::HARD_TURN: {
         /* Opposite wheel speeds */
         fSpeed1 = -m_sWheelTurningParams.MaxSpeed;
         fSpeed2 =  m_sWheelTurningParams.MaxSpeed;
         break;
      }
   }
   /* Apply the calculated speeds to the appropriate wheels */
   Real fLeftWheelSpeed, fRightWheelSpeed;
   if(cHeadingAngle > CRadians::ZERO) {
      /* Turn Left */
      fLeftWheelSpeed  = fSpeed1;
      fRightWheelSpeed = fSpeed2;
   }
   else {
      /* Turn Right */
      fLeftWheelSpeed  = fSpeed2;
      fRightWheelSpeed = fSpeed1;
   }
   /* Finally, set the wheel speeds */
   m_pcWheels->SetLinearVelocity(fLeftWheelSpeed, fRightWheelSpeed);
}

/****************************************/
/****************************************/

// Q learning

bool CFootBotForaging::SCollision::ShouldExploit() {
    static int explorePercentage = 100;
    int currentExploreChance = rand() % 100; //random number from 0-99
    if (currentExploreChance < explorePercentage) { //start at 100% chance
        explorePercentage--; //reduce chance for next time by 1%, after 100 explorations we only exploit
        return false;
	}
    return true;
}

int CFootBotForaging::SCollision::GetStratAmount() {
    return 4;
}

EStrategies CFootBotForaging::SCollision::GetRandomStrat() {
    return (EStrategies)(rand() % (GetStratAmount()));
}

EStrategies CFootBotForaging::SCollision::GetBestStrat() {
    EStrategies best = EStrategies::goLeft;
    if (Rewards[EStrategies::goRight] > Rewards[best])
        best = EStrategies::goRight;
    if (Rewards[EStrategies::backAndForth] > Rewards[best])
        best = EStrategies::backAndForth;
    if (Rewards[EStrategies::normalDodge] > Rewards[best])
        best = EStrategies::normalDodge;
    return best;
}

void CFootBotForaging::SCollision::ApplyReward() {
    auto collisionEnd = std::chrono::steady_clock::now();
    if (LastCollisionStart == NULL)
        return 0; //no collision to reward
    auto collisionTime = std::chrono::duration_cast<std::chrono::milliseconds>(collisionEnd - LastCollisionStart);
    if (AvgCollisionTime == NULL)
        AvgCollisionTime = collisionTime; //first collision is baseline, first reward is sacrificed as a result
    double rewardBase = (double)((AvgCollisionTime - collisionTime).count()); //the faster you get out, the better. being slower than avg gives neg reward.
    UpdateAvg(collisionTime);
    Rewards[CurrStrat] = GetNewAvg(Rewards[CurrStrat], LearningCounts[CurrStrat], rewardBase);
    LearningCounts[CurrStrat] += 1;
}

void CFootBotForaging::SCollision::UpdateAvg(std::chrono::milliseconds newTime) {
    AvgCollisionTime = std::chrono::milliseconds((int)GetNewAvg(AvgCollisionTime.count(), collisionCount, newTime.count()));
    collisionCount++;
}

double CFootBotForaging::SCollision::GetNewAvg(double currAvg, int count, double newVal) {
    return ((currAvg * count) + newVal) / (count + 1);
}

void CFootBotForaging::SCollision::Init() {
    AvgCollisionTime = NULL;
    LastCollisionStart = NULL;
    IsColliding = false;
    collisionCount = 0;
    LearningCounts = {
        {EStrategies::goLeft, 0},
        {EStrategies::goRight, 0},
        {EStrategies::backAndForth, 0},
        {EStrategies::normalDodge, 0}
	};
    Rewards = {
        {EStrategies::goLeft, 0.0},
        {EStrategies::goRight, 0.0},
        {EStrategies::backAndForth, 0.0},
        {EStrategies::normalDodge, 0.0}
	};
}

EStrategies CFootbotForaging::SCollision::Choose() {
    if (ShouldExploit())
        CurrStrat = GetBestStrat();
    else
        CurrStrat = GetRandomStrat();
    //...
    return CurrStrat;
}



/****************************************/
/****************************************/



/*THIS FUNCTION SEARCHING FOR PUCK/FOOD with a time limit for searching*/
void CFootBotForaging::findPuck() {
    bool bReturnToNest(false);
    /*
    * Test the first condition: have we found a food item?
    * NOTE: the food data is updated by the loop functions, so
    * here we just need to read it
    */
    if (m_sFoodData.HasFoodItem) {
        /* Store the result of the expedition */
        m_eLastExplorationResult = LAST_EXPLORATION_SUCCESSFUL;
        /* Switch to 'return to nest' */
        bReturnToNest = true;
    }
    /*If time for searching has passed*/
    else if(m_sStateData.TimeExploringUnsuccessfully > m_sStateData.MinimumUnsuccessfulExploreTime) {
        /* Store the result of the expedition */
        m_eLastExplorationResult = LAST_EXPLORATION_UNSUCCESSFUL;
        /* Switch to 'return to nest' */
        bReturnToNest = true;
    }
    /* So, do we return to the nest now? */
    if (bReturnToNest) {
        /* Yes, we do! */
        m_sStateData.TimeExploringUnsuccessfully = 0;
        m_pcLEDs->SetAllColors(CColor::YELLOW);
        m_sStateData.State = SStateData::STATE_RETURN_TO_NEST;
    } else {
        /* No, perform the actual exploration */
        ++m_sStateData.TimeExploringUnsuccessfully;
        UpdateState();
        /* Get the diffusion vector to perform obstacle avoidance */
        bool bCollision;
        /*
         * Here we check if collision occur , and initial robot direction as expected for
         * collision case , and non collision case.
         */
        CVector2 cDiffusion = DiffusionVector(bCollision);
       /*
       * If we are in the nest, we combine antiphototaxis with obstacle
       * avoidance
       * Outside the nest, we just use the diffusion vector
       */
        if(m_sStateData.InNest) {
            /*
             * we change state to leave home
             */
            m_pcLEDs->SetAllColors(CColor::WHITE);
            m_sStateData.State = SStateData::STATE_LEAVE_HOME;
        }
        else {
            /* Use the diffusion vector only */
            SetWheelSpeedsFromVector(m_sWheelTurningParams.MaxSpeed * cDiffusion);
        }
    }
}

/****************************************/
/****************************************/

void CFootBotForaging::goHome() {
    /*
     * As soon as you get to the nest, switch to 'resting' - here we
     * check the robot status comparing to his location and the home target
     */
    UpdateState();
    /* Are we in the nest? */
    if(m_sStateData.InNest) {
        /* Have we looked for a place long enough? */
        /* Yes, stop the wheels... */
        m_pcWheels->SetLinearVelocity(0.0f, 0.0f);
        /* Tell people about the last exploration attempt */
        m_pcRABA->SetData(0, m_eLastExplorationResult);
        /* ... and switch to state 'resting' */
        m_pcLEDs->SetAllColors(CColor::RED);
        m_sStateData.State = SStateData::STATE_RESTING;
        m_eLastExplorationResult = LAST_EXPLORATION_NONE;
        return;
    }
    /* Keep going */
    bool bCollision;
    SetWheelSpeedsFromVector(
        m_sWheelTurningParams.MaxSpeed * DiffusionVector(bCollision) +
            m_sWheelTurningParams.MaxSpeed * CalculateVectorToLight());
}

/****************************************/
/****************************************/
/*
 * Dropping the puck in the home location
 */
void CFootBotForaging::dropFood() {
    /* If we have stayed here enough switch to
    * leave home state */
    if(m_sStateData.TimeRested > m_sStateData.MinimumRestingTime) {
        m_pcLEDs->SetAllColors(CColor::WHITE);
        m_sStateData.State = SStateData::STATE_LEAVE_HOME;
        m_sStateData.TimeRested = 0;
    }
    else {
        ++m_sStateData.TimeRested;
        /* Be sure not to send the last exploration result multiple times */
        if(m_sStateData.TimeRested == 1) {
            m_pcRABA->SetData(0, LAST_EXPLORATION_NONE);
        }
    }
}
void CFootBotForaging::leaveHome() {
    UpdateState();
    /* Get the diffusion vector to perform obstacle avoidance */
    bool bCollision;
    /*
    * Here we check if collision occur , and initial robot direction as expected for
    * collision case , and non collision case.
    */
    CVector2 cDiffusion = DiffusionVector(bCollision);
    if(m_sStateData.InNest) {
        /*
         * The vector returned by CalculateVectorToLight() points to
         * the light. Thus, the minus sign is because we want to go away
         * from the light.
         */
        SetWheelSpeedsFromVector(
            m_sWheelTurningParams.MaxSpeed * cDiffusion -
                m_sWheelTurningParams.MaxSpeed * 0.25f * CalculateVectorToLight());
    } else {
        m_pcLEDs->SetAllColors(CColor::GREEN);
        m_sStateData.State = SStateData::STATE_EXPLORING;
    }
}


/****************************************/
/****************************************/

/*
 * This statement notifies ARGoS of the existence of the controller.
 * It binds the class passed as first argument to the string passed as
 * second argument.
 * The string is then usable in the XML configuration file to refer to
 * this controller.
 * When ARGoS reads that string in the XML file, it knows which controller
 * class to instantiate.
 * See also the XML configuration files for an example of how this is used.
 */
REGISTER_CONTROLLER(CFootBotForaging, "footbot_foraging_controller")
