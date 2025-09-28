/*
	Original author of the starter code
    Tanzir Ahmed
    Department of Computer Science & Engineering
    Texas A&M University
    Date: 2/8/20
	
	Please include your Name, UIN, and the date below
	Name: Rohak Brahma
	UIN: 934001579
	Date: 09/18/2025
*/
#include "common.h"
#include "FIFORequestChannel.h"
#include <sys/wait.h>

using namespace std;


int main (int argc, char *argv[]) {
	int opt;
	int p = -1;
	double t = -1;
	int e = -1;
	int m = MAX_MESSAGE;
	bool make_chan = false;
	vector<FIFORequestChannel*> channels;

	string filename = "";
	while ((opt = getopt(argc, argv, "p:t:e:f:m:c")) != -1) {
		switch (opt) {
			case 'p':
				p = atoi (optarg);
				break;
			case 't':
				t = atof (optarg);
				break;
			case 'e':
				e = atoi (optarg);
				break;
			case 'f':
				filename = optarg;
				break;
			case 'm':
				m = atoi(optarg);
				break;
			case 'c':
				make_chan = true;
				break; 
		}
	}

	// Give arguments to the server
	// Server needs './server', '-m', '<val for -m arg>', 'NULL'
	std::string m_val = std::to_string(m);
	char* cmd1[] = {(char*) "./server", (char*) "-m", (char*) m_val.c_str(), nullptr};
	// Fork
	pid_t pid = fork();
	if(pid == 0){
		// In the child run execvp using server arguments
		execvp(cmd1[0], cmd1);
		perror("execvp failure");
		exit(1);
	}

    FIFORequestChannel cont_chan("control", FIFORequestChannel::CLIENT_SIDE);
	channels.push_back(&cont_chan);

	if(make_chan){
		// Send new channel request to the server
		MESSAGE_TYPE nc = NEWCHANNEL_MSG;
    	cont_chan.cwrite(&nc, sizeof(MESSAGE_TYPE));
		// Create a variable to hold the name
		char new_name[MAX_MESSAGE];
		// cread the response from the server
		cont_chan.cread(new_name, MAX_MESSAGE);
		// Call the FIFORequestChannel constructor with the name from the server
		// Make new_chan a pointer so it's defined outside the loop
		FIFORequestChannel* new_chan = new FIFORequestChannel(new_name, FIFORequestChannel::CLIENT_SIDE);
		// Push the new channel into the vector
		channels.push_back(new_chan);
	}

	FIFORequestChannel chan = *(channels.back());

	char buf[MAX_MESSAGE]; // 256
	// Run this only if all 3 fields are provided
	if(p != -1 && t != -1 && e != -1){
		// example data point request
		datamsg x(p, t, e); // Change from harcode to user's values
		
		memcpy(buf, &x, sizeof(datamsg));
		chan.cwrite(buf, sizeof(datamsg)); // question
		double reply;
		chan.cread(&reply, sizeof(double)); //answer
		cout << "For person " << p << ", at time " << t << ", the value of ecg " << e << " is " << reply << endl;
	}

	// Else if p != -1, request 1000 datapoints
	else if(p != -1){
		std::ofstream outfile("received/x1.csv"); // Open file for output
		// Loop over 1st 1000 lines
		for(int i = 0; i < 1000; i++){
			double time = i*0.004; // Time is incremented by 0.004s
			// Send request for ecg 1
			datamsg x1(p, time, 1);
			memcpy(buf, &x1, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg));
			double ecg1;
			chan.cread(&ecg1, sizeof(double)); 
			// Send request for ecg 2
			datamsg x2(p, time, 2);
			memcpy(buf, &x2, sizeof(datamsg));
			chan.cwrite(buf, sizeof(datamsg));
			double ecg2;
			chan.cread(&ecg2, sizeof(double)); 
			// Write line to received/x1.csv
			outfile << time << ',' << ecg1 << ',' << ecg2 << '\n';
		}
		outfile.close(); // Close output file
	}

	if(!filename.empty()){
		// sending a non-sense message, you need to change this
		filemsg fm(0, 0);
		string fname = filename;
		
		int len = sizeof(filemsg) + (fname.size() + 1);
		char* buf2 = new char[len];
		memcpy(buf2, &fm, sizeof(filemsg));
		strcpy(buf2 + sizeof(filemsg), fname.c_str());
		chan.cwrite(buf2, len);  // I want the file length;

		int64_t file_size = 0;
		chan.cread(&file_size, sizeof(int64_t));

		// Create buffer of size buff capacity (m)
		char* buf3 = new char[m];

		// Create filemsg instance
		filemsg* file_req = (filemsg*)buf2;
		int64_t ofs = 0; // Set initial offset to 0
		// Create outfile treated as binary
		std::ofstream outfile(("received/" + filename).c_str(), std::ios::binary);
		// Loop over the segments in the file file_size / buff capacity (m)
		while(ofs < file_size){
			file_req->offset = ofs;
			file_req->length = std::min((int64_t)m, file_size - ofs);
			// Send the request (buf2)
			chan.cwrite(buf2, len); 
			// Receive the response
			// cread into buf3 length file_req->length
			chan.cread(buf3, file_req->length);
			// Write buf3 into file received/filename
			outfile.write(buf3, file_req->length);
			ofs += file_req->length;
		}
		delete[] buf2;
		delete[] buf3;
		outfile.close();
	}
	
	// If necessary, close and delete the new channel
	MESSAGE_TYPE msg = QUIT_MSG;
	if(make_chan){
		// New channel will be pushed to back of channels vector so delete from there
    	channels.back()->cwrite(&msg, sizeof(MESSAGE_TYPE));
		delete channels.back();
		channels.pop_back();
	}

	// Set chan to point at cont_chan
	chan = *(channels.back());

	// closing the channel    
    chan.cwrite(&msg, sizeof(MESSAGE_TYPE));

	wait(nullptr);
}