#include <boost/asio.hpp>
#include <kaa/Kaa.hpp>
#include <kaa/IKaaClient.hpp>
#include <kaa/configuration/manager/IConfigurationReceiver.hpp>
#include <kaa/configuration/storage/FileConfigurationStorage.hpp>
#include <kaa/log/strategies/RecordCountLogUploadStrategy.hpp>
#include <kaa/log/ILogStorageStatus.hpp>
#include <rtl-sdr.h>
#include "convenience/convenience.hpp"
#include <memory>
#include <string>
#include <cstdint>
#include <iostream>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdio.h>
#include <cstdlib>
#include <ncurses.h>
using namespace std;

#define DEFAULT_SAMPLE_RATE 2048000

#define SERVER_PORT htons(50007)

class ConfigurationCollection : public kaa::IConfigurationReceiver {
public:
  int serverSock;
  int clientSock;
  int n;
  bool rawData = false;
  int count = 0;

    ConfigurationCollection()
        : kaaClient_(kaa::Kaa::newClient())
        , samplePeriod_(0)
        , interval_(samplePeriod_)
        , timer_(service_, interval_)
    {
        // Set a custom strategy for uploading logs.
        kaaClient_->setLogUploadStrategy(
            make_shared<kaa::RecordCountLogUploadStrategy>(1, kaaClient_->getKaaClientContext()));
        // Set up a configuration subsystem.
        kaa::IConfigurationStoragePtr storage(
            make_shared<kaa::FileConfigurationStorage>(string(savedConfig_)));
        kaaClient_->setConfigurationStorage(storage);
        kaaClient_->addConfigurationListener(*this);
        auto handlerUpdate = [this](const boost::system::error_code& err)
        {
            this->update();
        };
        timer_.async_wait(handlerUpdate);
        //create_socket();
		rawData = true;
		if (rawData) set_up_device();
    }
	rtlsdr_dev_t *dev = NULL;

	void set_up_device() {
		static rtlsdr_dev_t *dev = NULL;
        int dev_index = 0;
		int r;

		dev_index = verbose_device_search("0");
		r = rtlsdr_open(&dev, (uint32_t)dev_index);
																				  if (r < 0) {
			printf("Failed to open device");
			exit (1);
		}
        verbose_set_sample_rate(dev, DEFAULT_SAMPLE_RATE);
        verbose_set_frequency(dev, 915800000);
        verbose_gain_set(dev, 1);
        verbose_ppm_set(dev, 0);
        verbose_reset_buffer(dev);
	}

    ~ConfigurationCollection()
    {
        // Stop the Kaa endpoint.
        kaaClient_->stop();
		close(serverSock);
        close(clientSock);
        cout << "Simple client demo stopped" << endl;
    }

    void run()
    {
        // Run the Kaa endpoint.
        kaaClient_->start();
        // Read default sample period
        samplePeriod_ = kaaClient_->getConfiguration().samplePeriod;
        const auto& topics = kaaClient_->getTopics();
	std::cout << "Topics available";
	for(const auto& topic : topics){
		std::cout << "Id : " << topic.id << ", name : " << topic.name;
	}
	cout << "\nDefault sample period: " << samplePeriod_<< std::endl;
        // Default sample period
        timer_.expires_from_now(boost::posix_time::seconds(samplePeriod_));
		kaa::KaaUserLogRecord logRecord;
	 
		// Push the record to the collector
		auto recordDeliveryCallback = kaaClient_->addLogRecord(logRecord);
	 
		try {
	    	auto recordInfo = recordDeliveryCallback.get();
	    	auto bucketInfo = recordInfo.getBucketInfo();
	        std::cout << "Received log record delivery info. Bucket Id [" <<  bucketInfo.getBucketId() << "]. "
        		<< "Record delivery time [" << recordInfo.getRecordDeliveryTimeMs() << " ms]." << std::endl;
		} catch (std::exception& e) {
			std::cout << "Exception was caught while waiting for callback result: " << e.what() << std::endl;
		}
        service_.run();
    }

private:
    static constexpr auto savedConfig_ = "saved_config.cfg";
    std::shared_ptr<kaa::IKaaClient> kaaClient_;
    int32_t samplePeriod_;
    boost::asio::io_service service_;
    boost::posix_time::seconds interval_;
    boost::asio::deadline_timer timer_;		

#define READ 0
#define WRITE 1

#define RAW_SIZE (8 * 1024) 
	int exec_raw(unsigned char *array) {
        uint32_t out_block_size = RAW_SIZE;
        int n_read = RAW_SIZE;
       
        int r = rtlsdr_read_sync(dev, array, out_block_size, &n_read);
        if (r < 0) {
			printf("Error in reading");
            return (1);
        }
		return (0);
	}
	
#define CHUNK_SIZE 1000

    void update()
    {
		/*if (kaaClient_->getKaaClientContext().getStatus().IKaaClientStateStoragePtr->getRecordsCount() > 10) {
			std::cout << "******* Hello **********";
			exit(1);
		} */
		if (!rawData) {
			char buffer[128];
			const char *cmd = "./rtl_power_fftw -n 32 -q -b 256 -f 916e6 -g 1";
			std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
			if (!pipe) throw std::runtime_error("popen() failed!");
				while (!feof(pipe.get())) {
					if (fgets(buffer, 128, pipe.get()) != nullptr) {
						stringstream ss;
						ss << buffer;
						kaa::KaaUserLogRecord logRecord;
						ss >> logRecord.frequency;
						ss >> logRecord.power;
						kaaClient_->addLogRecord(logRecord);
						std::cout << "Sampled power and frequency: " << logRecord.power << " " << logRecord.frequency << std::endl;
				}
			}
		}
		else {
			unsigned char *array = (unsigned char *)malloc(RAW_SIZE);
			exec_raw(array);	
			std::vector<unsigned char> vec(array, array + RAW_SIZE);
			kaa::KaaUserLogRecord logRecord;
			logRecord.frequency = logRecord.power = 0;
			logRecord.iq = vec;
			kaaClient_->addLogRecord(logRecord);

			std::cout << "IQ data entered: " << std::endl;
		}
	

        timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(samplePeriod_));
        // Posts the timer event
        //std::cout << "Calling update method:  " << endl;

        auto handlerUpdate = [this](const boost::system::error_code& err)
        {
            this->update();
        };
        timer_.async_wait(handlerUpdate);
    }

    void updateConfiguration(const kaa::KaaRootConfiguration &configuration)
    {
        std::cout << "Received configuration data. New sample period: "
            << configuration.samplePeriod << " seconds" << std::endl;
        samplePeriod_ = configuration.samplePeriod;
    }

    void onConfigurationUpdated(const kaa::KaaRootConfiguration &configuration)
    {
        updateConfiguration(configuration);
    }


};
int main()
{
    ConfigurationCollection configurationCollection;

    try {
        // It does control of the transmit and receive data
        configurationCollection.run();
    } catch (std::exception& e) {
        std::cout << "Exception: " << e.what();
    }
    return 0;
}
