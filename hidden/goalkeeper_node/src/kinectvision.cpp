#include "kinectvision.h"

/// \brief a mutex to avoid multi-thread access to message
pthread_mutex_t locmsg_mutex = PTHREAD_MUTEX_INITIALIZER;

kinectVision::kinectVision(ros::NodeHandle *par, fieldDimensions field)
{
    parent = par;
    save_mind = finalcounter = 0;
    currentField = field;
    gk_info_pub = parent->advertise<goalKeeperInfo>("goalKeeperInfo", 1);
    goodCandidateFound = false;
    configFilePaths();
    initVariables();
    if(!readParameters()) exit(0);
    generateLookUpTable();
    
    goal_line_x = (float)currentField.fieldDims.LENGTH/2000.0;
    side_line_y = (float)currentField.fieldDims.WIDTH/2000.0;
    _index = 0;
}

void kinectVision::publishData()
{
    gk_info_pub.publish(current_state);    
}
bool kinectVision::detectGameBall()
{
	static int fpsmeasure = 0,fpscounter = 0;

	int timing = 0;
	if(getImages(depthImage,rgbImage)){
		// Process Image
		filterByColor(true);
		filterByAnatomy(true);
		chooseBestCandidate(true);
		
		current_state.robot_info.ball_position.x = ballPosWorld.x;
		current_state.robot_info.ball_position.y = ballPosWorld.y;
		current_state.robot_info.ball_position.z = ballPosWorld.z;
		
		current_state.robot_info.ball_velocity.x = ballVelocities.x;
		current_state.robot_info.ball_velocity.y = ballVelocities.y;
		current_state.robot_info.ball_velocity.z = ballVelocities.z;
		
		
		if(ballPosition3D.z<0)current_state.robot_info.sees_ball = false;
		else current_state.robot_info.sees_ball = true;
   
        waitKey(5);
		return true;
	} else {
	    ROS_ERROR("Failed to read image");
		return false;
	}
}

void kinectVision::updateLocalizationData(Point3f pose, Point3f vels)
{
    pthread_mutex_lock (&locmsg_mutex); //Lock mutex
    
    current_state.robot_info.robot_pose.x = robotPose.x = pose.x;
    current_state.robot_info.robot_pose.y = robotPose.y = pose.y;
    current_state.robot_info.robot_pose.z = robotPose.z = pose.z;
    
    current_state.robot_info.robot_velocity.x = vels.x;
    current_state.robot_info.robot_velocity.y = vels.y;
    current_state.robot_info.robot_velocity.z = vels.z;
    
    pthread_mutex_unlock (&locmsg_mutex); //Unlock mutex        
}

double kinectVision::calculate_margin(int RunningMargin)
{

    double margin=0;
    margin= 640- refinedCandidates[save_mind][0] - RunningMargin;

    if (margin < (refinedCandidates[save_mind][0] - RunningMargin)){
        margin = refinedCandidates[save_mind][0] - RunningMargin ;
    }

    if (margin < (refinedCandidates[save_mind][1]-RunningMargin)){
        margin = refinedCandidates[save_mind][1]-RunningMargin;
    }
    
    if (margin < (480 - refinedCandidates[save_mind][1]- RunningMargin)){
        margin = 480 - refinedCandidates[save_mind][1];
    }
    margin /= 10;

    return margin;
}

double kinectVision::checkLimits( int mode , double RunningMargin){  // function to make sure that 0<nrows<480   and 0<ncols<640

    switch (mode) {
    case 1:
        if ((refinedCandidates[save_mind][1] - RunningMargin)<0){
            return 0; 
        }else{
            return (refinedCandidates[save_mind][1] - RunningMargin);
        }
        break;
    case 2:
        if ((refinedCandidates[save_mind][1] + RunningMargin)>480){
            return 480;
        }else {
            return (refinedCandidates[save_mind][1] + RunningMargin);
        }
        break;
    case 3:
        if ((refinedCandidates[save_mind][0] - RunningMargin)<0){
            return 0;
        } else {
            return (refinedCandidates[save_mind][0] - RunningMargin);
        }
        break;
    case 4:
        if ((refinedCandidates[save_mind][0] + RunningMargin)>640){
            return 640;
        } else {
            return (refinedCandidates[save_mind][0] + RunningMargin);
        }
        break;
    default: break;
    }

}

double kinectVision::updateLimits (int mode=1, int minId=0)
{
//sabendo que a bola tem tamanho real x então podemos utilizar isso para definir quantas vezes maior será o pedaço a seguir, por exemplo:
//a bola mede m metros e p pixeis, a uma velocidade v ela anda k*x metros em 30 milisegundos, logo anda k*p pixeis da camara,
//logo para seguir a bola a margem deverá ser de 1,5*k*p pixeis, sendo que k*x neste caso representa a v maxima que a bola poderá atingir (deve incluir uma pequena margem)

/*

ObjSizePix - tamanho da bola em pixeis
RunningMargin - margem em pixeis calculada para uma velocidade x a multiplicar por uma margem de y, vai ser o tamanho da janela que se deve pesquisar em x
*/	//static bool found_the_ball=false;
	static int lastballPositionY=0, counter=0,RunningMargin=640;
	double ObjSizePix=0;
	static double Margin = 480 ;
	
	if (!mode) {
		save_mind=minId;
		//found_the_ball=ball_found;
		return 0;
	} else{
		
        if(!goodCandidateFound) {
            if (counter == 20){
                counter=20;
            } else {
                counter++;
            }
            if (counter == 11 ) Margin=calculate_margin(RunningMargin);
        } else{
            counter=0;
            //refinedCandidates[minID][2]; /raio em pixeis da bola que em cm é aproximadamente 11 cm 

            if(ballPosition3D.y != lastballPositionY){

                ObjSizePix = 2 * refinedCandidates[save_mind][2];//Get the size of the object in pixels,by multiplying the radius by two
                RunningMargin = 0.35*ObjSizePix/0.222;//By using the ball diameter (22.2cm) and the max velocity margin (40cm for 48km/h(13,3 m/s) with a margin) we get the velocity margin in pixels


            }	
        }
        
        lastballPositionY=ballPosition3D.y;

        finalcounter=counter-10; // after 10 frames of 30 ms the margin 
        if (finalcounter<0) 
        finalcounter=0; 
        return RunningMargin+(Margin*finalcounter);	// valor em pixeis
	}
	
	return 640;

}

Point2d kinectVision::mapPointToWorld(double rx, double ry, double angle, float dist, double theta, double offset)
{
    // theta must be in radians
    // rx,ry,dist,offset must be in meters
    // angle must be in degrees
    double pointRelX = -dist*cos(theta);
    double pointRelY = dist*sin(theta);
    double ang = angle*(M_PI/180.0);
    double angOffset = ang;
    return Point2d(rx-cos(ang)*pointRelX-sin(ang)*pointRelY+(offset*sin((angOffset))),
                   ry-sin(ang)*pointRelX+cos(ang)*pointRelY-(offset*cos((angOffset))));
}

void kinectVision::filterByColor(bool show)
{
    candidates.clear();
    clear.copyTo(binary);
    Vec3b* pixel;
    uchar *pixelB;
    long int index = 0;
    double RunningMargin=640,teste=0;	
    static bool start = true;	
    	
	if(start){
		
		for(int i = 0; i < nRows; ++i){
        		pixel = rgbImage.ptr<Vec3b>(i);
        		pixelB = binary.ptr<uchar>(i);
        		for (int j = 0; j<nCols; ++j){
           			 index = (pixel[j][2]<<16) + (pixel[j][1]<<8) + (pixel[j][0]);
           			 if ( YUVLookUpTable[index] == UAV_ORANGE_BIT )  pixelB[j] = 255;//se é bola
        			 if(goodCandidateFound)		
					start= false;		     
						     }
    					      }
			
		}
	
	else{
		RunningMargin = updateLimits(1);


		for(int i = (checkLimits(1,RunningMargin)) ; i < (checkLimits( 2,RunningMargin)) ; ++i){
			pixel = rgbImage.ptr<Vec3b>(i);
        		pixelB = binary.ptr<uchar>(i);
			for (int j = (checkLimits(3,RunningMargin)); j< (checkLimits( 4,RunningMargin)) ; ++j){
				index = (pixel[j][2]<<16) + (pixel[j][1]<<8) + (pixel[j][0]);
				if ( YUVLookUpTable[index] == UAV_ORANGE_BIT )  pixelB[j] = 255;//se é bola
        		}
    		}
		if(show) rectangle(rgbImage, Point (checkLimits( 4,RunningMargin),checkLimits( 2,RunningMargin)), Point (checkLimits(3,RunningMargin),checkLimits(1,RunningMargin)), Scalar( 255, 255, 255 ));
		// copy rgbImage to toSendRgbImage if requested
		
		
	}	

    morphologyEx(binary,binary,MORPH_DILATE, elementdil);
    morphologyEx(binary,binary,MORPH_ERODE, elementero);
    
    if(show)imshow("ColorSegmentation",binary);
    // copy binary to toSendBinary if requested
    
    vector<Mat> contours; Moments moment;
    Point2f center; float radius;
    findContours(binary, contours,CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0, 0));
    for (unsigned int i = 0; i < contours.size(); i++){
        if (contourArea(contours[i])>300  && contourArea(contours[i])<30000 ){ // tweak contour area
            moment = moments(contours[i], true);
            double area = moment.m00;
            double x = moment.m10 / area; // calculate the center of mass
            double y = moment.m01 / area;

            Rect rBound = boundingRect(contours[i]); // calculate min enclosing circle and rectangle
            minEnclosingCircle((Mat)contours[i], center, radius);

            double wh = fabs(1 - ((double)rBound.width / rBound.height)); // calculate circle ratios
            double pi = fabs(1 - (fabs(area)) / (CV_PI * pow(radius, 2)));
            if (wh<=0.91 - (0.001 * finalcounter) && pi<=0.81 - (0.01 * finalcounter) ){   // errrrrrrrooooo
                candidates.push_back(Point3i(x,y,radius));
            }
            
         }
    }
}

void kinectVision::filterByAnatomy(bool show)
{
	vector<Vec4i> filteredCandidates;
    filteredCandidates.clear();	
    int x = 0, y = 0, radius = 0; double z = 0.0;
    uchar *pixelB;	
    
    for(unsigned int i=0;i<candidates.size();i++){
		x = candidates[i].x; y = candidates[i].y; radius = candidates[i].z;
		pixelB = depthImage.ptr<uchar>(y);
		z = pixelB[x];
		if((z>0 && z<245)) {
		    filteredCandidates.push_back(Vec4i(x,y,radius,z));
		}
    }
    
    uchar *candidate; refinedCandidates.clear();
    Mat roi; int dim = 0; int range = 0;
    for(unsigned int c=0;c<filteredCandidates.size();c++){
        x = filteredCandidates[c][0]; y = filteredCandidates[c][1]; dim = filteredCandidates[c][2]+range;
        z = filteredCandidates[c][3];
        pixelB = depthImage.ptr<uchar>(y);
        z = pixelB[x];
        roi = Mat(dim*2,dim*2,CV_8UC1,Scalar(0));

        for(int i=-dim;i<=dim/1.2;i++){
            pixelB = depthImage.ptr<uchar>(i+y);
            candidate = roi.ptr<uchar>(i+dim);
            for(int j=-dim;j<=dim;j++){
                if(pixelB[j+x] != 0 && pixelB[j+x] != 255){
                    if(isInRange(z,pixelB[j+x])){
                        candidate[j+dim] = 255;
                    }
                }
            }
        }

        //Verify if object
        morphologyEx(roi,roi,MORPH_DILATE, elementdil);
        morphologyEx(roi,roi,MORPH_ERODE, elementero);
        //imshow("ROI",roi);
        vector<Mat> contours; Moments moment; Point2f center; float radius;
        findContours(roi, contours,CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE, Point(0,0));

        for (unsigned int i = 0; i < contours.size(); i++){
            if (contourArea(contours[i])>300   && contourArea(contours[i])<30000){ // tweak contour area
                moment = moments(contours[i], true);
                double area = moment.m00;
                double _x = moment.m10 / area; // calculate the center of mass
                double _y = moment.m01 / area;

                Rect rBound = boundingRect(contours[i]); // calculate min enclosing circle and rectangle
                minEnclosingCircle((Mat)contours[i], center, radius);

                double wh = fabs(1 - ((double)rBound.width / rBound.height)); // calculate circle ratios
                double pi = fabs(1 - (fabs(area)) / (CV_PI * pow(radius, 2)));
                if (wh<=0.7 - (0.02*finalcounter) && pi<=0.7- (0.02*finalcounter)  ){   ///// erro proporcional to the size of the image
                    refinedCandidates.push_back(Vec6d((x-dim)+_x,(y-dim)+_y,radius,z,area,0.0));
                }
             }
        }

    }
    
    if(show) { // remove later
    	/*for(unsigned int i=0;i<filteredCandidates.size();i++){
			//circle(rgbImage,Point(filteredCandidates[i][0],filteredCandidates[i][1]),filteredCandidates[i][2],
			Scalar(255,0,0),2);
		}
		for(unsigned int i=0;i<refinedCandidates.size();i++){
			//circle(rgbImage,Point(refinedCandidates[i][0],refinedCandidates[i][1]),refinedCandidates[i][2],
			//Scalar(0,255,0),3);
		}*/
		imshow("Candidate",rgbImage); 
    }
    
}
	
void kinectVision::chooseBestCandidate(bool show)
{
    double devArea = 0.0, devRads = 0.0; //Deviations
    double mz = 0; double minSum = 10.0; int minID = 0; double minZ = 0.0;
    double tilt = 1.5; double xCand = 0.0, yCand = 0.0; double proximityRatio = 0.0;
    //bool goodCandidateFound = false;
	goodCandidateFound =false;
    //get Z, morphological z operation, choose best
    
    for(unsigned int cand=0;cand<refinedCandidates.size();cand++){
        mz = getZ(refinedCandidates[cand][3]); // Real Z in meters
        devArea = abs(1-(refinedCandidates[cand][4]/getStdArea(mz)));
        devRads = abs(1-(refinedCandidates[cand][2]/getStdRads(mz)));
		
		if(ballPosition3D.z>=0){
			xCand = mz+0.125+0.1;		
			yCand = tan(((refinedCandidates[cand][0]-320)*convHorizontal)*(M_PI/180.0))*mz;
			proximityRatio = sqrt((xCand-ballPosition3D.x)*(xCand-ballPosition3D.x)+(yCand-ballPosition3D.y)*(yCand-ballPosition3D.y))/1.0;
		} else proximityRatio = 0.0;

        if((devRads+devArea) < minSum && (devRads+devArea)<= 0.7 - (0.01 * finalcounter) && proximityRatio<=1.7 - (0.02 * finalcounter)) { 
// se estiver entre a area e o raio da bola goodCandidateFound ;
        	goodCandidateFound = true;

            minSum = (devRads+devArea);
            minID = cand;
            minZ = mz;
	    updateLimits(0,minID);  ///////// update inID
	   	
		
        }
    }
/* AQUIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIIII o candidatoooooo*/ 
	if(refinedCandidates.size()>0 && goodCandidateFound){
		ballPosition3D.x = minZ+0.125+0.1;
		ballPosition3D.y = tan(((refinedCandidates[minID][0]-320)*convHorizontal)*(M_PI/180.0))*minZ;
		ballPosition3D.z = 0.52+minZ*tan((M_PI/180.0)*(6-(refinedCandidates[minID][1]-240)*convVertical))+0.11;	
		
		Point2d aux = mapPointToWorld(0,0,robotPose.z,sqrt((ballPosition3D.x*ballPosition3D.x)+
		(ballPosition3D.y*ballPosition3D.y)),atan2(ballPosition3D.y,ballPosition3D.x)+M_PI_2,0);
		
		ballPosWorld.x = robotPose.x+aux.x;
		ballPosWorld.y = robotPose.y+aux.y;
		ballPosWorld.z = ballPosition3D.z;
		
		// Calculate elapsed distance and elapsed time
		double elapsedDist = sqrt((lastBallPosition3D.x-ballPosition3D.x)*(lastBallPosition3D.x-ballPosition3D.x)
			+(lastBallPosition3D.y-ballPosition3D.y)*(lastBallPosition3D.y-ballPosition3D.y));
	    gettimeofday(&t2, NULL);
		double deltaTime = (t2.tv_sec - t1.tv_sec) * 1000.0;      // sec to ms
        deltaTime += (t2.tv_usec - t1.tv_usec) / 1000.0;   // us to ms
        deltaTime/=1000.0;
		
		if((elapsedDist/deltaTime)<=7.0){ // 7m/s top speed
			//Assign initial velocities
			ballVelocities = Point3d((ballPosWorld.x-lastBallPosWorld.x)/deltaTime,(ballPosWorld.y-lastBallPosWorld.y)/deltaTime
				,(ballPosWorld.z-lastBallPosWorld.z)/deltaTime);
			//Predict impact zone
			predictBallPosition();
		} else {
			ballVelocities = Point3d(0,0,0);
			ballImpactZone = Point3d(0,0,-1);	
		}

		//Assign to last
		lastBallPosition3D = ballPosition3D;
		lastBallPosWorld = ballPosWorld;
		//timerVel.start();
		gettimeofday(&t1, NULL);
				
	} else ballPosition3D = Point3d(0,0,-1);
	
	/// draw on toSendRgbImage
	if(refinedCandidates.size()>0 && goodCandidateFound && show){///////////////////////// valoooooooooreeessss xyeraio
		circle(rgbImage,Point(refinedCandidates[minID][0],refinedCandidates[minID][1]),
		refinedCandidates[minID][2],Scalar(255,0,255),2);
	}
 	if(show) imshow("Candidate",rgbImage);   
}

bool kinectVision::getImages(Mat &depth_,Mat&rgb_)
{
	void *depth,*rgb;
	unsigned int _timestamp;
	if(freenect_sync_get_depth(&depth,&_timestamp,_index,FREENECT_DEPTH_REGISTERED)==0
	 && freenect_sync_get_video(&rgb,&_timestamp,_index,FREENECT_VIDEO_RGB)==0){
	 	//Retrieve video Image
	 	Mat rgbMat(Size(640,480),CV_8UC3,Scalar(0));
        uint8_t* rgb_data = static_cast<uint8_t*>(rgb);
        rgbMat.data = rgb_data;
        cvtColor(rgbMat, rgb_, CV_RGB2BGR);
	 	//Retrieve depth Image
	 	Mat depthMat(Size(640,480),CV_16UC1);
	 	uint16_t* depth_data = static_cast<uint16_t*>(depth);
	 	depthMat.data = (uchar *)depth_data;
	 	depthMat.convertTo(depth_, CV_8UC1, 255.0/4096.0);
	 }	
}

void kinectVision::configFilePaths()
{
    std::string home = std::string(getenv("HOME"));
    configFolderPath = home+std::string(COMMON_PATH);
    configFolderPath += std::string(KIN_CFG_PATH);
	lutFile = configFolderPath+std::string(KINLUTFILENAME);
	paramFile = configFolderPath+std::string(KINPARAMSFILENAME);
}

void kinectVision::generateLookUpTable()
{
 	/*if(start){
 		QFile file_(lutFile);
		if(!file_.open(QIODevice::WriteOnly)) {
		    ROS_ERROR("Error writing to %s",KINLUTFILENAME);
		    return;
		} QTextStream in_(&file_);
		in_ << QString("HRANGES="+QString::number(LUTRanges[0])+","+QString::number(LUTRanges[1])+"\n");
		in_ << QString("SRANGES="+QString::number(LUTRanges[2])+","+QString::number(LUTRanges[3])+"\n");
		in_ << QString("VRANGES="+QString::number(LUTRanges[4])+","+QString::number(LUTRanges[5])+"\n");
		file_.close();
	}*/
    //Generates in memory look up table
	int y,u,v;
    rgb pix; hsv pix2;
    unsigned int index;
    for (int r=0; r<256; r++) // classify every RGB color into our LUT
        for (int g=0; g<256; g++)
            for (int b=0; b<256; b++)
            {
                pix.r = r; pix.g = g; pix.b = b;
                pix2 = rgbtohsv(pix);
                y = pix2.h;
                u = pix2.s;
                v = pix2.v;
                index = (r<<16)+(g<<8)+b;

                //-- initialize on update --
                YUVLookUpTable[index] = UAV_NOCOLORS_BIT;
                //-- Reference Colour range -UAV_GREEN_BIT-
                if (((y>=LUTRanges[0]) && (y<=LUTRanges[1])) && ((u>=LUTRanges[2]) && (u<=LUTRanges[3])) &&
                          ((v>=LUTRanges[4]) && (v<=LUTRanges[5]))){
                    YUVLookUpTable[index] = UAV_ORANGE_BIT;
                }
            }

}

void kinectVision::initVariables()
{
	double morph_size_dil = 0.5;
	double morph_size_erode = 2.0;
	
	Mat rgbImage(Size(640,480),CV_8UC3,Scalar(0));
	nRows = 480; nCols = 640;

    convHorizontal = (57/2.0)/320.0; 
    convVertical = (43/2.0)/240.0;
    
    elementdil = getStructuringElement(2, Size( 2*morph_size_dil + 1, 2*morph_size_dil+1 ), Point( morph_size_dil, morph_size_dil ) );
    elementero = getStructuringElement(2, Size( 2*morph_size_erode + 1, 2*morph_size_erode+1 ), Point( morph_size_erode, morph_size_erode ) );
    
    clear = Mat (480,640,CV_8UC1,Scalar(0));
    
	ballImpactZone = Point3d(0,0,-1);
    ballVelocities = Point3d(0,0,0);
    ballPosition3D = Point3d(0,0,-1);
}
   
bool kinectVision::isInRange(int z, int val)
{
    int range = 7;
    if(abs(val-z)<range) {
        return true;
    }
    else return false;
}
 
std::vector<std::string> kinectVision::split(const std::string& s, char seperator)
{
   std::vector<std::string> output;

    std::string::size_type prev_pos = 0, pos = 0;

    while((pos = s.find(seperator, pos)) != std::string::npos)
    {
        std::string substring( s.substr(prev_pos, pos-prev_pos) );

        output.push_back(substring);

        prev_pos = ++pos;
    }

    output.push_back(s.substr(prev_pos, pos-prev_pos)); // Last word

    return output;
}

bool kinectVision::readParameters()
{
    heightFromGround = 0.7;
    std::ifstream file(paramFile);
    if(!file.is_open()) {
        ROS_ERROR("Error reading %s",KINPARAMSFILENAME);
        return false;
    }
	
	//Init height from ground
    std::string height; getline(file,height);
    heightFromGround = std::stof(height.substr(height.find("="),height.size()-1));
	
	//Init FOV
    std::string fov; getline(file,fov);
    fov = fov.substr(fov.find("="),fov.size()-1);
    
    
    std::vector<std::string> fovs = split(fov,',');
    hFov = std::stof(fovs[0]);
    vFov = std::stof(fovs[1]);
    file.close();
    
    std::ifstream file_(lutFile);
    if(!file_.is_open()) {
        ROS_ERROR("Error reading %s",KINLUTFILENAME);
        return false;
    }
    
    memset(LUTRanges,0,6*sizeof(int));
    std::string h; getline(file_,h);
    std::vector<std::string> hValues = split(h.substr(h.find("="),h.size()-1),',');
    if(hValues.size()!=2 || h=="") { ROS_ERROR("Error in H %s file",KINLUTFILENAME); exit(0); }
    LUTRanges[0] = std::stoi(hValues[0]); LUTRanges[1] = std::stoi(hValues[1]);
    
    std::string s; getline(file_,h);
    std::vector<std::string> sValues = split(s.substr(s.find("="),s.size()-1),',');
    if(sValues.size()!=2 || s=="") { ROS_ERROR("Error in S %s file",KINLUTFILENAME); exit(0); }
    LUTRanges[2] = std::stoi(sValues[0]); LUTRanges[3] = std::stoi(sValues[1]);
    
    std::string v; getline(file_,h);
    std::vector<std::string> vValues = split(v.substr(v.find("="),v.size()-1),',');
    if(vValues.size()!=2 || v=="") { ROS_ERROR("Error in V %s file",KINLUTFILENAME); exit(0); }
    LUTRanges[4] = std::stoi(vValues[0]); LUTRanges[5] = std::stoi(vValues[1]);
    
    file_.close();
    
    return true;
}

double kinectVision::getZ(int pixVal)
{
    if(pixVal<=28) return 0.5;
    else if(pixVal>=255) return 4.0;
    else {
        return (-0.000000263321223161165*pow(pixVal,3))
                +(0.00009535747359655*pow(pixVal,2))
                +(0.0076242311*pixVal)
                +0.2354778415+0.02;
    }
}

double kinectVision::getStdArea(double mVal)
{
    if(mVal==0.5) return 43000;
    else if(mVal==4) return 650;
    else {
        return (9871.3954736085*pow(mVal,-2.0397879024));
    }
}

double kinectVision::getStdRads(double mVal)
{
    if(mVal==0.5) return 125;
    else if(mVal==4) return 16;
    else {
        return (61.2541340469*pow(mVal,-0.9949180346));
    }
}

// Prediction
bool kinectVision::intrestingEventHappened(double x, double y, double xi)
{
    if(y>=side_line_y || y<=-side_line_y) return true; // ball goes through lateral lines
    else if(fabs(x)>=goal_line_x && (y>=0.9 || y<=-0.9)) return true; // shot wide
    else if(fabs(x)<=goal_line_x+0.3 && fabs(x)>=goal_line_x-0.3 && y<=0.9 && y>=-0.9) return true;
    else return false;
}

bool kinectVision::crossGoalLine(double x)
{
    x = fabs(x);
    if(x<=goal_line_x+0.3 && x>=goal_line_x-0.3) return true;
    else return false;
}

void kinectVision::predictBallPosition()
{
    double Xf, Yf, Zf, huehue=0, maxHeight=0, XfAux, YfAux;
    bool potentialGoal=false, lowShot=false;
	double g = 9.8;
	double Xi = ballPosWorld.x, Yi = ballPosWorld.y, Zi = ballPosWorld.z;
	double Vz = ballVelocities.z;
	double e = 0.88;
	int N = 10;
	bool rside = true;
	if(robotPose.x<0) rside = false;
    if((rside && ballVelocities.x<0.9) || (!rside && ballVelocities.x>-0.9)) { ballImpactZone = Point3d(0,0,-1); return;} 
    for(int i=0; i<N; i++)//while
        {
        double t=(-Vz-sqrt((Vz)*(Vz) + 4*ballPosWorld.z*0.5*g))/(2*-0.5*g);

        for(double j=0; j<=t; j+=0.01){
            Xf=Xi + ballVelocities.x*j; //Computes new predicted point
            Yf=Yi + ballVelocities.y*j;
            Zf=Zi+ Vz*j - 0.5*g*(j*j);

            if(maxHeight<Zf){ maxHeight=Zf; XfAux=Xf; YfAux=Yf; } // Assign max height
            if(intrestingEventHappened(Xf,Yf,Xi)){ potentialGoal=true; break;} // Detect events
        }

        if(maxHeight<=0.45) { lowShot=true; break; } // If it is a low shot, predict continuous movement
        maxHeight=0;

        if(!crossGoalLine(Xf) && potentialGoal) { // No solution on prediction and end of pred.
			ballImpactZone = Point3d(0,0,-1);
            break;
        } else if(crossGoalLine(Xf) && potentialGoal) { // Solution on prediction
            ballImpactZone = Point3d(Xf,Yf,Zf);
            break;
        } else { // Continue prediction
            Xi=Xf;
            Yi=Yf;
            Zi=Zf;
            Vz=Vz*e;
        }
     }

    if(lowShot){   
        potentialGoal=false;
        double m=(Yf-YfAux)/(Xf-XfAux);
        double b=Yf-m*Xf;
        double xPredTarget = goal_line_x;
        if(!rside) xPredTarget *= -1.0;
	    double new_y=m*xPredTarget+b;
        if(new_y<=0.9&&new_y>=-0.9){
		   ballImpactZone = Point3d(xPredTarget,new_y,0);
        } else ballImpactZone = Point3d(0,0,-1);
    }
    
    current_state.impact_zone.x = ballImpactZone.x;
    current_state.impact_zone.y = ballImpactZone.y;
    current_state.impact_zone.z = ballImpactZone.z;
    
}

hsv kinectVision::rgbtohsv(rgb in)
{
    hsv temp;
    int min = 0, max = 0, delta = 0;
    if(in.r<in.g)min=in.r; else min=in.g;
    if(in.b<min)min=in.b;

    if(in.r>in.g)max=in.r; else max=in.g;
    if(in.b>max)max=in.b;

    temp.v = max;                // v, 0..255
    delta = max - min;                      // 0..255, < v

    if(max != 0)
        temp.s = (int)(delta)*255/max;        // s, 0..255
    else {
        // r = g = b = 0        // s = 0, v is undefined
        temp.s = 0;
        temp.h = 0;
        return temp;
    }
    if(delta==0) temp.h = 0;
    else {
        if( in.r == max )
            temp.h = (in.g - in.b)*30/delta;        // between yellow & magenta
        else if( in.g == max )
            temp.h = 60 + (in.b - in.r)*30/delta;    // between cyan & yellow
        else
            temp.h = 120 + (in.r - in.g)*30/delta;    // between magenta & cyan

        while( temp.h < 0 ) temp.h += 180;
    }

    if(temp.h>160){
        temp.h = (int)(-0.11111*temp.h)+20;
    }
    return temp;
}
