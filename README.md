# Kaa Application to collect the distributed spectrum data
[![Build Status]

## SETUP steps:
1. Fire up the KAA sandbox in the virtualbox
2. Login with default username - kaa and password - kaa
3. Navigate to http://130.245.144.129:9080/sandbox/ and make sure kaa sandbox is running 

## Folder SpecSense
1. The database and schema have been configured in the sandbox. 
	In order to change any of the configurations, follow the steps from Change Schema onwards [here](https://kaaproject.github.io/kaa/docs/v0.10.0/Programming-guide/Your-first-Kaa-application/). 
        Download the new SDK and replace the code in main.cpp with your own application code

2. The client code runs on the odroid board and the sandbox is running on the desktop server(ideally)
3. To run the code on the client side(either on Odroid or the on the desktop (for speed):
	a. Navigate into Documents/SpecSense
	b. Follow the steps below :
		cd build
		cmake -DKAA_MAX_LOG_LEVEL=3 ..
		make
	c. ./kaa-app
	d. If successful data should start getting populated in the MongoDB database of the server.

4. The sandbox is running on the server side. To look at the data being stored in the configured database ( in our case MongoDB):
	a. Fetch the Application token of the application from the Sandbox UI : 43346047488248758778
	b. Navigate to the sandbox shell running on the virtualbox and type the following commands
		$ mongo kaa
		$ db.logs_<application_token>.find()

## Contributors
**Snigdha Kamal, Jitendra Savanpur, Mallesham Dasari, Arani Bhattacharya**

## Useful Links
To manipulate the data types inside the schemas and a list of possible datatypes available-
https://docs.kaaproject.org/display/KAA/Configuration
