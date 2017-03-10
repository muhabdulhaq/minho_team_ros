#include "rstriker.h"

/* What rSTRIKER Does:
Parking - Goes to parking spot 2
Own Kickoff - Receives the ball passed by rSUPSTRIKER
Their Kickoff - Stays midfield
Own Freekick - Received the ball passed by rSUPSTRIKER and shoots if possible
Their Freekick - Stays between the ball and the goalie, trying to grab it
*/

RoleStriker::RoleStriker() : Role(rSTRIKER)
{
   kick_dists.clear();
   kick_strengths.clear();
   for(float d=0.0;d<=6.0;d+=0.5) kick_dists.push_back(d);
   kick_strengths.push_back(100.0);
   kick_strengths.push_back(100.0); kick_strengths.push_back(100.0);
   kick_strengths.push_back(75.0); kick_strengths.push_back(73.0);
   kick_strengths.push_back(78.0); kick_strengths.push_back(81.0);
   kick_strengths.push_back(83.0); kick_strengths.push_back(88.0);
   kick_strengths.push_back(93.0); kick_strengths.push_back(96.0);
   kick_strengths.push_back(100.0); kick_strengths.push_back(100.0);
}

RoleStriker::~RoleStriker()
{
}

void RoleStriker::setRosNodeHandle(ros::NodeHandle *parent, std::string topic_base)
{

}

void RoleStriker::determineAction()
{
   if(mBsInfo.gamestate==sSTOPPED) { kicked_after_recv = ball_already_passed = false; mAction = aSTOP; }
   else if(mBsInfo.gamestate==sPARKING||
           mBsInfo.gamestate==sPRE_OWN_KICKOFF||
           mBsInfo.gamestate==sPRE_THEIR_KICKOFF||
           mBsInfo.gamestate==sPRE_OWN_FREEKICK){
      // Slow motion for parking or precise positioning
      mAction = aAPPROACHPOSITION;
   }else if(mBsInfo.gamestate==sOWN_KICKOFF||
            mBsInfo.gamestate==sOWN_FREEKICK){ 
      // In this cases sSTRIKER has to receive the ball
      if(mRobot.has_ball) mAction = aKICKBALL; // replace by kick ball
      else if(kicked_after_recv) mAction = aSTOP;
      else mAction = aRECEIVEBALL;
   }else {
      // For now, do aFASTMOVE for every other game state
      mAction = aFASTMOVE;
   }
}

void RoleStriker::computeAction(aiInfo *ai)
{
   mBsInfo.posxside = !mBsInfo.posxside; // Temporary, to test against running goalkeeper
   determineAction();
   (*ai) = aiInfo(); 
   ai->role = mRole;
   ai->action = mAction;  
   static int stab_counter = 0;
   static int initialPlay = true;

   if(mAction==aSTOP)return;

   switch(mBsInfo.gamestate){
      case sPARKING:{
         // Go to parking spot
         if(mBsInfo.posxside) ai->target_pose.x = 1.6;
         else ai->target_pose.x = -1.6;
         ai->target_pose.y = side_line_y+0.5;
         ai->target_pose.z = 180.0;
         break;
      }      
      case sPRE_OWN_KICKOFF:{
         // Go to edge of big circle, wait to receive the ball
         ball_already_passed = initialPlay = false;
         stab_counter = 0;
         ai->target_pose.x = 0.0;
         if(mBsInfo.posxside){
            ai->target_pose.y = center_circle_radius;
            ai->target_pose.z = 180.0;
         } else {
            ai->target_pose.y = -center_circle_radius;  
            ai->target_pose.z = 0.0; 
         }
         break;
      }
      case sOWN_KICKOFF:{
         // After play is given, rSUPSRIKER has to pass the
         // ball to rSTRIKER, so, rSTRIKER has to "align" with the ball
         float tarx = mRobot.ball_position.x,
               tary = mRobot.ball_position.y;
         if(!mRobot.sees_ball){
            //Assume ball is at 0.0, 0.0
            tarx = 0.0;
            tary = 0.0;   
         }

         if(initialPlay){
            initialPlay = false;
            kick_spot.x = mRobot.ball_position.x;
            kick_spot.y = mRobot.ball_position.y;
         }
         
         float distFromKickSpot = sqrt((kick_spot.x-mRobot.ball_position.x)*
         (kick_spot.x-mRobot.ball_position.x)
         +(kick_spot.y-mRobot.ball_position.y)*
         (kick_spot.y-mRobot.ball_position.y));

         ai->target_pose.x = tarx; 
         if(mBsInfo.posxside){
            ai->target_pose.y = center_circle_radius;
            ai->target_pose.z = 180.0;
         } else {
            ai->target_pose.y = -center_circle_radius;  
            ai->target_pose.z = 0.0; 
         }
         
         if(mRobot.has_ball){
            mAction = aHOLDBALL;
            ai->target_pose = mRobot.robot_pose; //stay still and hold ball
            // basestation should switch state to own_game
            ball_already_passed = false;
         } else {
            float ball_velocity = sqrt
            (mRobot.ball_velocity.x*mRobot.ball_velocity.x+
            mRobot.ball_velocity.y*mRobot.ball_velocity.y);

            if(!ball_already_passed && distFromKickSpot>0.5 &&
            ball_velocity>0.25){
               ball_already_passed = true;
            }
            
            bool ball_opdir = true;
            if(mBsInfo.posxside){
               if(mRobot.ball_velocity.y>0.35) ball_opdir = false; 
            } else {
               if(mRobot.ball_velocity.y<-0.35) ball_opdir = false;  
            }              

            if(ball_already_passed &&
            ((distFromKickSpot>0.5)&&(ball_velocity<0.25||ball_opdir))
            || fabs(mRobot.ball_position.y)>=fabs(mRobot.robot_pose.y)){
               mAction = aENGAGEBALL;
               ai->target_pose = mRobot.ball_position;
            }            
         }
         break;  
      }
      case sPRE_THEIR_KICKOFF:
      case sTHEIR_KICKOFF:{
         // Go to middle half of left/right field and rotate towards ball
         if(mBsInfo.posxside){
            ai->target_pose.x = center_circle_radius;
            ai->target_pose.y = side_line_y/2.0;
         } else {
            ai->target_pose.x = -center_circle_radius;
            ai->target_pose.y = -side_line_y/2.0;  
         }
         
         float tarx = 0.0, tary = 0.0;
         if(mRobot.sees_ball){
            tarx = mRobot.ball_position.x;
            tary = mRobot.ball_position.y;
         }
         ai->target_pose.z = orientationToTarget(tarx,tary);
         break;
      }
      case sPRE_OWN_FREEKICK:{
         kicked_after_recv = ball_already_passed = initialPlay = false;
         if(mBsInfo.posxside){
            ai->target_pose.x = mRobot.ball_position.x+1.0;
            ai->target_pose.y = mRobot.ball_position.y;
            ai->target_pose.z = 90.0;
         } else {
            ai->target_pose.x = mRobot.ball_position.x-1.0;
            ai->target_pose.y = mRobot.ball_position.y;
            ai->target_pose.z = 270.0; 
         }     
         break; 
      }
      case sOWN_FREEKICK:{
         if(initialPlay){
            initialPlay = false;
            kick_spot.x = mRobot.ball_position.x;
            kick_spot.y = mRobot.ball_position.y;
         }
         float distFromKickSpot = sqrt((kick_spot.x-mRobot.ball_position.x)*
         (kick_spot.x-mRobot.ball_position.x)
         +(kick_spot.y-mRobot.ball_position.y)*
         (kick_spot.y-mRobot.ball_position.y));

         if(mRobot.has_ball){ // kick to goal
            float shoot_tarx = goal_line_x;
            float shoot_tary = 0.0;
            if(mBsInfo.posxside) shoot_tarx = -goal_line_x;

            if(mRobot.has_ball) stab_counter++;
            else stab_counter = 0;
            //check if path is clear
            if(pathClearForShooting(shoot_tarx,&shoot_tary)){
               if(stab_counter<=3) { mAction = aHOLDBALL; ai->target_pose = mRobot.robot_pose; break; }
               stab_counter = 0;

               ai->target_pose.x = mRobot.robot_pose.x;
               ai->target_pose.y = mRobot.robot_pose.y;
               ai->target_pose.z = orientationToTarget(shoot_tarx,shoot_tary);

               ai->target_kick_strength = getKickStrength(shoot_tarx,
               shoot_tary);
            }else {
               mAction = aDRIBBLEBALL; ai->target_pose = mRobot.robot_pose;
               // adjust robot position and heading to keep ball trajectory
               // out of contact with another robot
               float displacement_left = 0.0, displacement_right = 0.0;
               getObstaclesDisplacement(&displacement_left,&displacement_right);

               if(!mBsInfo.posxside){
                  if(displacement_left>=displacement_right) 
                     ai->target_pose.y-=0.5*displacement_left; 
                  else ai->target_pose.y+=0.5*displacement_right; 
               } else {
                  if(displacement_left>=displacement_right) 
                     ai->target_pose.y+=0.5*displacement_left; 
                  else ai->target_pose.y-=0.5*displacement_right;
               }
               
            }
         } else { // receive ball
            float ball_velocity = sqrt
            (mRobot.ball_velocity.x*mRobot.ball_velocity.x+
            mRobot.ball_velocity.y*mRobot.ball_velocity.y);

            if(!ball_already_passed && distFromKickSpot>0.5 &&
            ball_velocity>0.25){
               ball_already_passed = true;
            }
            
            bool ball_opdir = true;
            if(mBsInfo.posxside){
               if(mRobot.ball_velocity.x>0.35) ball_opdir = false; 
            } else {
               if(mRobot.ball_velocity.x<-0.35) ball_opdir = false;  
            }              

            if(ball_already_passed &&
            ((distFromKickSpot>0.5&&distFromKickSpot<1.5)&&(ball_velocity<0.25||ball_opdir))
            || fabs(mRobot.ball_position.x)<=fabs(mRobot.robot_pose.x)){
               mAction = aENGAGEBALL;
               ai->target_pose = mRobot.ball_position;
            } else {
               ai->target_pose.x = mRobot.robot_pose.x; 
               ai->target_pose.y = mRobot.ball_position.y;
               ai->target_pose.z = orientationToTarget
               (mRobot.ball_position.x,mRobot.ball_position.y);
            }
             
         }

         break;
      }

      case sPRE_THEIR_FREEKICK:{
         if(mRobot.sees_ball){
            double thresh_distance = 3.0; 
            // stay in a line between the ball and the center of the goalie
            // at the allowed distance
            float tarx = goal_line_x, tary = 0.0;
            if(!mBsInfo.posxside) tarx*=-1;
            float path_direction = atan2(tary-mRobot.ball_position.y,
                  tarx-mRobot.ball_position.x);
            ai->target_pose.x = mRobot.ball_position.x+thresh_distance*cos(path_direction);
            ai->target_pose.y = mRobot.ball_position.y+thresh_distance*sin(path_direction);
            
            //check if we are inside of owen penalty area due to thresh_distance
            //if we are, move ourselves to the matching spot on area's edge
            if(fabs(ai->target_pose.x)>big_area_x){
               float m = (tary- mRobot.ball_position.y)/(tarx- mRobot.ball_position.x);
               float b = tary-m*tarx;
               bool intercepts_sides = false;
               float hsw = m*big_area_x*1.5;
               float hsh = big_area_y/m;
               float hh = big_area_x*1.5;
               float hw = big_area_y;
               if(-hh <= hsw && hsw <= hh) intercepts_sides = true;

               if(!intercepts_sides){
                  ai->target_pose.y = big_area_y;
                  if(mRobot.ball_position.y<0)ai->target_pose.y*=-1;
                  ai->target_pose.x = (ai->target_pose.y-b)/m;
               }else {
                  ai->target_pose.x = big_area_x;
                  if(!mBsInfo.posxside)ai->target_pose.x*=-1;
                  ai->target_pose.y = ai->target_pose.x*m+b;
               }  
            }
            ai->target_pose.z = orientationToTarget
            (mRobot.ball_position.x,mRobot.ball_position.y);
         } else {
            mAction = aSTOP;
         }          
         break;
      }
      


      default: { mAction = aSTOP; break; }
   }
      
   ai->action = mAction; 
}

std::string RoleStriker::getActiveRoleName()
{
   return std::string("rSTRIKER");
}

void RoleStriker::setField(fieldDimensions fd)
{  
   field = fd;
   goal_line_x = (float)field.fieldDims.LENGTH/2000.0;
   side_line_y = (float)field.fieldDims.WIDTH/2000.0;
   small_area_x = goal_line_x-(float)field.fieldDims.AREA_LENGTH1/1000.0;
   small_area_y = (float)field.fieldDims.AREA_WIDTH1/2000.0;
   big_area_x = goal_line_x-(float)field.fieldDims.AREA_LENGTH2/1000.0;
   big_area_y = (float)field.fieldDims.AREA_WIDTH2/2000.0;
   center_circle_radius = (float)field.fieldDims.CENTER_RADIUS/1000.0;
}

bool RoleStriker::pathClearForShooting(float tarx, float *tary)
{
   float a = 0.0, c=0.0, gapdistance=0.0, best_meandist = 0.0, meandist = 0.0, nosolmean_dist = 0.0;
   vec2d near,col;
   std::vector<vec2d> colpoints;
   bool hasSolution = false;
   for(float targety=-0.9; targety<=0.9; targety+=0.1){
      a = (targety-mRobot.ball_position.y)/(tarx-mRobot.ball_position.x);// m in y=mx+b   
      c = mRobot.ball_position.y-a*mRobot.ball_position.x;// b in y=mx+b
      colpoints.clear();
      //get nearest point from every detected obstacle to current shooting path
      meandist = 0.0;
      for(int i=0;i<mRobot.obstacles.size();i++){
         col.x = mRobot.obstacles[i].x; col.y = mRobot.obstacles[i].y;
         near = getNearestPointToPath(a,c,col);
         // see if ball can go through between the robot and the shot's path
         gapdistance = sqrt((col.x-near.x)*(col.x-near.x)+(col.y-near.y)*(col.y-near.y));
         meandist += gapdistance;
         if(gapdistance<=0.5) colpoints.push_back(col);
      }
      
      meandist/=(float)mRobot.obstacles.size();

      if(!colpoints.size()){
         hasSolution = true;
         if(meandist>best_meandist){
            (*tary)=targety;
            best_meandist = meandist;
         }
      } else {
         if(meandist>nosolmean_dist){
            collisionPoints = colpoints;
            nosolmean_dist = meandist;
         }
      }
   }
   
   return hasSolution;
}

vec2d RoleStriker::getNearestPointToPath(float a, float c, vec2d point)
{
   float b = -1;
   vec2d col;
   col.x = (b*(b*point.x-a*point.y)-a*c)/(a*a+b*b);
   col.y = (a*(-b*point.x+a*point.y)-b*c)/(a*a+b*b);
   return col;
}

void RoleStriker::getObstaclesDisplacement(float *left,float *right)
{
   float weight = 0.0,asbx = 0.0, m = 0.0;
   for(int i=0;i<mRobot.obstacles.size();i++){
      if(mRobot.obstacles[i].y>=-1.5 && mRobot.obstacles[i].y<=1.5){ 
         asbx = fabs(mRobot.obstacles[i].x);

         m = 1/(goal_line_x-mRobot.ball_position.x);
         weight = m*asbx+(1-goal_line_x*m);

         if(!mBsInfo.posxside){   
            if(mRobot.obstacles[i].x>= mRobot.robot_pose.x){
               (*left) += fabs(-1.0-mRobot.obstacles[i].y)*weight;
               (*right) += fabs(1.0-mRobot.obstacles[i].y)*weight;
            }
         } else {
            if(mRobot.obstacles[i].x<= mRobot.robot_pose.x){
               (*right) += fabs(-1.0-mRobot.obstacles[i].y)*weight;
               (*left) += fabs(1.0-mRobot.obstacles[i].y)*weight;
            }
         }
      }
   }
}

int RoleStriker::getKickStrength(float tarx, float tary)
{
   int index = 0;
   float dist = sqrt((tarx-mRobot.robot_pose.x)*(tarx-mRobot.robot_pose.x)
                +(tary-mRobot.robot_pose.y)*(tary-mRobot.robot_pose.y));
   while(dist>=kick_dists[index]) index++;
   
   if(index>kick_dists.size()-1) return 100;
   double m = (kick_strengths[index]-kick_strengths[index-1])/
              (kick_dists[index]-kick_dists[index-1]);
   
   return m*dist+(kick_strengths[index]-kick_dists[index]*m);
}