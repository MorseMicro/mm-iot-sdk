/*
 * Copyright 2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief AWS IoT example to demonstrate connecting to AWS Shadow service and demonstrate a
 *        simple light bulb example.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 *
 * This example application demonstrates how to use AWS IoT Core services to connect to an
 * AWS endpoint and use the shadow service to mirror the state of a light bulb shadow state
 * maintained by AWS.
 *
 * # Getting Started {#AWS_GETTING_STARTED}
 *
 * The following steps need to be done to run this application:
 *
 * ## Sign in to the AWS IoT console
 *
 * - Go to https://aws.amazon.com/ and click _Sign in_ on the top right corner.
 *   If you don't already have an Amazon/AWS account, create one or contact your company IT
 *   department to create a company account for you.
 * - From the services menu on the top left select _Internet of Things > IoT Core_
 *
 *
 * ## Create a Thing {#CREATE_THING}
 *
 * - On the left hand menu select _Manage > All Devices > Things_
 * - Click _Create things_
 * - Select _Create single thing_
 * - Click _Next_
 * - Enter a Thing name. You will need to remember this for name later.
 * - Select _Unnamed shadow (classic)_
 * - Enter the following as shadow statement:
 *   @code
 *   {
 *       "state": {
 *           "reported": {
 *               "powerOn": 0
 *           },
 *           "desired": {
 *               "powerOn": 0
 *           }
 *       }
 *   }
 *   @endcode
 * - Click _Next_
 * - Select _Auto generate new certificates_
 * - Click _Next_.
 *
 * - Now create an "Allow All" policy if you do not already have one (you only need to do this
 *   once).
 *    - Click _Create Policy_ (this will open a new tab)
 *    - Enter a policy name such as @c AllowAll
 *    - In the _Builder_ tab, in _Policy document_ enter the following data:
 *      @code
 *      Policy effect:   Allow
 *      Policy action:   *
 *      Policy resource: *
 *      @endcode
 *    - Click _Create_.
 *    - Return to the _Attach policies to certificate_ tab in your browser
 * - Ensure the @c AllowAll policy is selected
 * - Click _Create thing_
 *
 * Congratulations! Your device should now be created.
 *
 * Download the certificates and key files and proceed to the next step. Specifically you will
 * need the Device Certificate, Device Private Key amd the Amazon _RSA_ Root Certificate.
 *
 * @note You can also create the Allow All policy on the AWS CLI using the following command:
 * @code
 * aws iot create-policy \
 *    --policy-name="AllowAllDev" \
 *    --policy-document="{ \"Version\": \"1\", \
 *                         \"Statement\": [{ \
 *                              \"Effect\": \"Allow\", \
 *                              \"Action\": \"iot:*\", \
 *                              \"Resource\": \"*\" \
 *                       }]}"
 * @endcode
 * Likewise, you can create all certificates in the AWS CLI. You may also use third party tools
 * like @c openssh to create the device certificate and keys and upload them using AWS CLI.
 *
 * @note We show creating certificates for a single thing using the AWS IoT console as this is
 * the easiest way to test the application. This is however not a practical way to provision
 * devices in the field. For production we recommend Just In Time Provisioning as documented
 * here @c https://docs.aws.amazon.com/iot/latest/developerguide/jit-provisioning.html
 * This method loads all devices with a common provisioning certificate, which then gets
 * replaced with a unique device certificate when the device first connects to the AWS
 * endpoint.
 *
 *
 * ## Install device certificates and keys {#INSTALL_CREDENTIALS}
 *
 * - In the credentials directory replace the files @c AmazonRootCA.pem, @c certificate.pem.crt,
 *   and @c private.pem.key with the files downloaded from AWS in the previous step. Rename the
 *   downloaded files to match the target file names. Note: Amazon provides you with 2 root CA
 *   files to download, please use the file named @c AmazonRootCA1.pem.
 * - Open the @c config.hjson file.
 *   - Update the value of @c aws.thingname with the name of the thing you created in the
 *     previous section.
 *   - Now, in AWS IoT console, navigate to Manage > All Devices > Things
 *     - Click on the thing you just created and then click on the "Device Shadows" tab.
 *     - Click on the "Classic Shadow". Then look at the "Device Shadow URL" in the
 *       "Device Shadow details" panel.
 *     - Update the value of @c aws.endpoint with the server name in the URL. Ensure you remove the
 *       @c https:// and everything after @c amazonaws.com
 *       - Alternatively, you can get the endpoint from the AWS CLI by typing in the following
 *         command:
 *         @code
 *         aws iot describe-endpoint
 *         @endcode
 * - Follow the @ref MMCONFIG_PROGRAMMING instructions to load the @c config.hjson file to the
 *   device.
 * @note The default config store key names are defined in @ref aws_iot_config.h. You may change
 * these names as required for your application.
 *
 *
 * ## Running the application
 *
 * - Now build and run the application.
 * - You should see the device connect to AWS IoT.
 *   - If you see connection failures ensure your access point forwards traffic to the Internet.
 * - Once the device is connected look for the state of the @c powerOn variable.
 *   - Initially it should be 0 signifying the lamp is OFF - in the ST Nucleo boards this is
 *     represented by the Blue LED which should be OFF.
 * - Now Open the AWS IoT console and click on the "MQTT Test client" from the left hand side menu.
 *   - Click on "Publish to a Topic" tab and enter the following topic:
 *     @code
 *     $aws/things/<your_thing_name>/shadow/update
 *     @endcode
 *     Naturally, replace @c "<your_thing_name>" with the name of the thing you created.
 *   - Enter the following JSON text in the "Message payload" section and click "Publish".
 *     @code
 *     {
 *        "state": {
 *	          "desired": {
 *  	          "powerOn": 1
 *	          }
 *        }
 *     }
 *     @endcode
 * - You should now see the LED turn ON and the value of @c powerOn parameter
 *   printed on the console output should change.
 *
 *
 * # What is AWS Shadow?
 *
 * AWS Shadow service allows us to cache the state of an IoT device on the cloud.
 * This allows the IoT device to go to sleep and wake up once in a while to update
 * its state on the cloud and act on pending state change requests on the cloud.
 * For example a temperature sensor could periodically measure and update the temperature
 * using the AWS Shadow service and go to sleep for minutes or even hours at a time
 * while AWS Shadow caches this data and makes it available persistently to anyone who
 * may be interested in the temperature even though the sensor is asleep and no longer
 * reachable on the network.
 *
 * Likewise, for a light bulb, AWS Shadow remembers the state of the light bulb even
 * though the light bulb itself may have been unplugged and so not connected to the
 * network. In fact the AWS Shadow can accept state change requests on behalf of
 * the light bulb when it is unplugged and then convey the desired state to the
 * light bulb when it connects again.
 *
 * This is how it works:
 *
 * - A device such as a light switch that desires to change the state of the light bulb
 *   publishes a message to the topic @c "$aws/things/<your_thing_name>/shadow/update"
 *   @code
 *   {
 *      "state": {
 *	        "desired": {
 *  	        "powerOn": 1
 *	        }
 *      }
 *   }
 *   @endcode
 *
 * - If this is a change in state, then AWS Shadow will post a delta message to
 *   @c "$aws/things/<your_thing_name>/shadow/update/delta" as
 *   @code
 *   {
 *      "state": {
 *  	    "powerOn": 1
 *      }
 *   }
 *   @endcode
 *   - If the device is unplugged, then AWS Shadow will automatically send this message with
 *     the latest state when the device comes online next.
 * - Once the device receives this delta message and acts on it, it will then report its new
 *   state by publishing to: @c "$aws/things/<your_thing_name>/shadow/update"
 *   @code
 *   {
 *      "state": {
 *	        "reported": {
 *  	        "powerOn": 1
 *	        }
 *      }
 *   }
 *   @endcode
 * - If the state change was accepted, AWS Shadow will forward the above message to:
 *   @c "$aws/things/<your_thing_name>/shadow/update/accepted"
 * - If for some reason, the state change request was rejected by the device, then AWS Shadow
 *   will publish to @c "$aws/things/<your_thing_name>/shadow/update/rejected"
 *
 * For more information see:
 * @c https://docs.aws.amazon.com/iot/latest/developerguide/iot-device-shadows.html
 *
 * # Integrating with Alexa
 * Now that we have seen AWS Shadow being used to connect to and control our IoT device,
 * how can we put this to practice with a real world application?
 *
 * Amazon shows us how we can use Alexa to post messages to AWS IoT devices with an example
 * smart hotel application:
 * @c https://aws.amazon.com/blogs/iot/implement-a-connected-building-with-alexa-and-aws-iot/
 *
 * In this example they show us how to create an Alexa skill that can post JSON messages
 * to our IoT device using AWS Shadow. You can use the tutorial above to customize this
 * application for your specific smart device needs.
 *
 * # Automation and Production
 * In the example above we used the AWS IoT console to create certificates and publish/subscribe
 * to MQTT messages. While this may be convenient for development and testing, it is hardly a
 * practical approach for real world applications. Luckily, we have options - Amazon provides us
 * with the AWS CLI (Command Line Interface), which lets us do everything above using a command
 * line interface such as creating certificates, registering devices, publishing and subscribing
 * to MQTT messages. This allows us to automate and script production and registration of
 * devices and even implement M2M communications using the MQTT publish/subscribe API. A full
 * reference of the available commands is at: @c https://docs.aws.amazon.com/cli/latest/index.html
 *
 *
 * # OTA Updates
 *
 * This application supports Over The Air updates. OTA updates can be used to update the existing
 * application on the IoT device with newer versions of itself. For OTA updates to work, there must
 * be a bootloader installed and an existing version of the application that is able to connect to the
 * AWS IoT infrastructure.
 *
 * To perform an OTA update, first ensure there is a version of this application running on the
 * device with the right credentials and certificates to connect to the service.  In addition to
 * certificates and keys described in step 3 of the Getting Started section above, we will
 * also need a code signing certificate to be installed on the device. This certificate can be
 * generated using a tool like openssl as described below:
 *
 * @note OTA update feature is supported only on @c mm-ekh08-u575 and @c mm-mm6108-ekh05 platforms - you
 * will get an error on all other platforms if you attempt an OTA update. To add OTA support to
 * your platform, you will need to:
 * - Build and install the bootloader for your platform. (See @ref bootloader.c) If you are
 *   loading the application using @c openocd, then _first_ load the application and then load
 *   the bootloader. Do this every time you load the application using openocd as the
 *   application contains a stub bootloader that will overwrite the real bootloader.
 * - Ensure you have enabled flash file-system support for your platform. To add flash
 *   file-system support for your platform, you will need to add a @c .filesystem section
 *   to your linker definition file that is big enough to accommodate the OTA download image
 *   plus any other data you may store in the file-system. Also ensure you have a valid
 *   implementation of @ref mmhal_get_littlefs_config() that returns your file-system
 *   configuration.
 *
 * ## Generating Code Signing Keys
 *
 * ### Option 1: Generating @c ECDSA Keys for signing by AWS
 * The steps below are from the following document:
 * @c https://docs.aws.amazon.com/freertos/latest/userguide/ota-code-sign-cert-win.html
 * - In your working directory, create a file named @c cert_config.txt with the following text:
 *   @code
 *   [ req ]
 *   prompt             = no
 *   distinguished_name = my_dn
 *   [ my_dn ]
 *   commonName = me@myemail.com
 *   [ my_exts ]
 *   keyUsage         = digitalSignature
 *   extendedKeyUsage = codeSigning
 *   @endcode
 *   Replace @c me@myemail.com with your organization email and replace the my in @c my_dn
 *   and @c my_exts with your organization name as appropriate.
 *
 * - Create an @c ECDSA code-signing private key:
 *   @code
 *   openssl genpkey -algorithm EC -pkeyopt ec_paramgen_curve:P-256 -pkeyopt ec_param_enc:named_curve -outform PEM -out otasigning.key
 *   @endcode
 * - Create an @c ECDSA code-signing certificate:
 *   @code
 *   openssl req -new -x509 -config cert_config.txt -extensions my_exts -nodes -days 365 -key otasigning.key -out otasigning.pem.crt
 *   @endcode
 * - Copy the @c otasigning.pem.crt file to the credentials folder and follow the
 *   @ref MMCONFIG_PROGRAMMING instructions to load the @c config.hjson file to the device.
 *   Ensure you have generated the other certificates, keys and provisioning information
 *   as described in @ref AWS_GETTING_STARTED section above.
 *
 * ### Option 2: Generating Self signing (RSA) keys
 * - Create the signing keys & certificate as shown below:
 *   @code
 *   openssl genrsa -out otasigning.key 2048
 *   openssl rsa -in otasigning.key -pubout otasigning.pem.crt
 *   @endcode
 * - Copy the @c otasigning.pem.crt file to the credentials folder and follow the
 *   @ref MMCONFIG_PROGRAMMING instructions to load the @c config.hjson file to the device.
 *   Ensure you have generated the other certificates, keys and provisioning information
 *   as described in @ref AWS_GETTING_STARTED section above.
 * - Sign the code
 *  - Build the update file for the application, you should get a .mbin file in the output directory
 *    @code
 *    make mbin -j4
 *    @endcode
 *  - Sign the .mbin file as shown below
 *    @code
 *    openssl dgst -sha256 -sign otasigning.key -out signature.bin <MBIN file>
 *    openssl enc -base64 -in signature.bin -out signature.txt
 *    @endcode
 * - Upload the MBIN file to AWS as described below, paste the contents of the @c signature.txt
 *   file into the signature field

 * ## Deploying the update
 * - First, ensure your AWS account has the required permissions. For more information, see:
 *   @c https://docs.aws.amazon.com/freertos/latest/userguide/code-sign-policy.html
 * - In the AWS IoT Core portal, select Manage > Remote Actions > Jobs, then click Create job.
 * - Under Job type select Create FreeRTOS OTA update job, then choose Next.
 * - Enter a name for the job and click Next.
 * - Under Devices to update, choose one or more things or thing groups from the drop down menu.
 * - Select @c MQTT for file transfer protocol.
 * - Under Sign and choose your file, select Sign a new file for me.
 * - If you are using Option 2 (Self signed image), then select Use My custom signed file
 *  - Cut and paste the contents of the @c signature.txt file that you generated while signing
 *    into the signature box.
 *  - Select SHA-256 as the original hash algorithm.
 *  - Select RSA as the original encryption algorithm.
 *  - Enter @c aws.ota_cert as the path name of the code signing certificate on the device.
 *    (This incidentally is the name of the config store key that contains the certificate)
 *  - Click Upload a new file and upload the .mbin file.
 *  - If you have not already done so, create an S3 bucket and specify the path to save the
 *    file as.
 *  - In path name of the file on the device enter the name of the mbin file.  Ensure this is a
 *    valid file name or the update will fail.
 *  - Select an @c IAM role with permissions to do OTA updates and click Next.
 *  - In the next screen click Create Job.
 * - If you are using Option 1 (Signing by AWS)
 *  - If you have not already done so, under Code signing profile, choose Create new profile.
 *   - In Create a code signing profile, enter a name for your code-signing profile.
 *   - Under Device hardware platform choose Windows Simulator.
 *     You will need to get your platform certified by AWS for it to appear here.
 *   - Select Import a new code signing certificate.
 *   - Upload @c otasigning.pem.crt as the certificate body.
 *   - Upload @c otasigning.key as the certificate private key.
 *   - Click import.
 *   - Enter @c aws.ota_cert as the path name of the code signing certificate on the device.
 *   - Click Create.
 *  - Back at the previous screen, select the code signing profile you just created.
 *  - Click Upload a new file and upload the .mbin file.
 *  - If you have not already done so, create an S3 bucket and specify the path to save the
 *    file as.
 *  - In path name of the file on the device enter the name of the mbin file.  Ensure this is a
 *    valid file name or the update will fail.
 *  - Select an @c IAM role with permissions to do OTA updates and click Next.
 *  - In the next screen click Create Job.
 *
 * Congratulations! The update has been deployed.  If the application is currently running on
 * the device, it should immediately see the update and start downloading it. In a minute or two
 * the device should reboot and the new update will be applied.  AWS will indicate successful
 * update on the console once the updated firmware boots and connects to AWS.
 *
 * @note If the above process does not work for you, then you can debug the issue by enabling
 * OTA logging by removing the @c DISABLE_OTA_LOGGING flag from the @c Makefile and
 * @c platformio.ini files for this example. Also, ensure you increment the version numbers in
 * @c version.h prior to every OTA update or you will get warnings that the new version is not
 * higher than the old version, the update will still complete though.
 *
 * For more information, see:
 * @c https://docs.aws.amazon.com/freertos/latest/userguide/ota-console-workflow.html
 *
 *
 * # Fleet Provisioning
 *
 * The steps above describe how to provision one device at a time for AWS IoT. This however is
 * not practical when we have to provision thousands of devices in the field. Thankfully AWS
 * provides a mechanism called Fleet Provisioning that allows us to automate this process.
 *
 * For more information on Fleet Provisioning see:
 * @c https://docs.aws.amazon.com/whitepapers/latest/device-manufacturing-provisioning/provisioning-identity-in-aws-iot-core-for-device-connections.html
 *
 * There are multiple mechanisms of implementing Fleet Provisioning - we have implemented the
 * _Fleet Provisioning by claim_ mechanism. In this mechanism a batch of devices (say 1000) will
 * be loaded with a shared claim certificate and keys. The device will make first contact with
 * the AWS endpoint using this shared certificate and key just as it would with regular device
 * certificates and keys. The device then generates a new set of keys and sends the certificate
 * to AWS for signing. AWS signs the certificate and assigns the device a new thing name based
 * on the provisioning template - the provisioning template may specify the thing name is an
 * amalgam of the device serial number (in our case the MAC address) and some prefix. The device
 * saves the signed certificate, generated keys and thing name and uses them for all connections
 * henceforth.
 *
 * Use the steps below to implement fleet provisioning. Alternatively, if you prefer to use the
 * AWS command line, then all the steps below are described here for the command line:
 * @c https://www.freertos.org/iot-fleet-provisioning/demo.html#setting-up
 *
 * ## Prerequisites
 * Ensure you have the required permissions and roles to implement fleet provisioning. Your AWS
 * administrator can setup the roles using the following commands on the AWS command line:
 * - Create @c FleetProvisioningRole role
 *   @code
 *   aws iam create-role --role-name "FleetProvisioningRole" --assume-role-policy-document \
 *       '{"Version":"2012-10-17","Statement":[{"Action":"sts:AssumeRole","Effect":"Allow", \
 *       "Principal":{"Service":"iot.amazonaws.com"}}]}'
 *   @endcode
 * - Attach @c AWSIoTThingsRegistration policy to the role
 *   @code
 *   aws iam attach-role-policy --role-name "FleetProvisioningDemoRole" \
 *       --policy-arn arn:aws:iam::aws:policy/service-role/AWSIoTThingsRegistration
 *   @endcode
 * - Create the @c AllowAll policy as described in @ref CREATE_THING
 *
 * ## Create a thing type {#CREATE_THING_TYPE}
 * - Open the AWS IoT console web page.
 * - Go to _Manage > All Devices > Thing types_
 * - Click _Create thing type_
 * - For this example, enter @c _fp_demo_things_ and click _Create thing type_
 * - If you change the name remember to update it in the JSON file in the step below.
 *
 * ## Create a claim policy {#CREATE_CLAIM_POLICY}
 * - On the AWS IoT console go to _Manage > Security > Policies_
 * - Click _Create policy_ and give it a suitable name
 * - In the _Policy document_ section, click JSON and enter the code below (which can also be
 *   found in `templates/fleet_provisioning_policy.json`):
 *   @include aws_iot/templates/fleet_provisioning_policy.json
 *   Replace <aws-region> and <aws-account-id> as appropriate for your AWS account.
 *   Ensure you replace @c _FleetProvisioningTemplate_ with the name of the template you
 *   specified in @ref CREATE_PROV_TEMPLATE. Set the @c aws.provisioningtemplate key
 *   to the provisioning template name you specified here.
 *
 * ## Create provisioning template {#CREATE_PROV_TEMPLATE}
 * - On the AWS IoT console go to _Connect > Connect many devices > Connect many devices_
 * - Click _Create provisioning template_
 * - Select _Provisioning devices with claim certificate_ then click _Next_
 * - Select _Active_, then enter a template name such as @c _FleetProvisioningTemplate_
 * - Select the provisioning role you created in the prerequisites step above. Ensure
 *   _Attach managed policy to @c IAM role_ is unchecked.
 * - Under _Claim certificate policy_ select the claim certificate you created in
 *   above, Then click _Next_.
 * - In the next page select _Don't use a pre-provisioning action_, then click _Next_.
 * - In the next page click _Next_.
 * - Finally click _Create template_
 * - Then click on the newly created template, then click _Edit JSON_ in the document
 *   section below. Enter the following JSON  (which can also be found in
 *   `templates/fleet_provisioning_template.json`):
 *   @include aws_iot/templates/fleet_provisioning_template.json
 *   Ensure @c _AllowAll_ matches the name of the @c AllowAll policy you created in the prerequisites.
 *   Ensure @c _fp_demo_things_ matches the thing type you created in @ref CREATE_THING_TYPE
 * - Click _Save as new version_
 *
 *
 * ## Generate claim certificates and keys
 * - On the AWS IoT console go to _Manage > Security > Certificates_
 * - Click Add Certificate > Create Certificate
 * - On the next page download the private key, certificate and the root certificate.
 * - Use this certificate and key as the claim certificate for a batch of devices.
 *   You may create multiple certificates here, one for each batch.
 * - Select the certificate(s) you just created then click _Attach policy_ and attach
 *   the claim policy you created earlier to all the claim certificates
 *   you generated now.
 *
 * ## Setting up the device
 * - Once the above steps are done, load the claim certificate, private key and root certificate
 *   as described in @ref INSTALL_CREDENTIALS. The device will use these credentials for the
 *   initial connection and will be replaced by device certificate and keys provided by
 *   AWS when provisioning has successfully completed.
 * - Set the @c aws.provisioningtemplate key to the provisioning template name you
 *   set in @ref CREATE_PROV_TEMPLATE.
 * - Ensure the device has the necessary @c Wi-Fi credentials and country code.
 * - Power cycle the device, it should connect to AWS with the provided claim certificate.
 *   AWS will then sign the new certificate generated by the device and provide it with a Thing
 *   Name that the device will write to @c aws.thingname and atomically replace the claim
 *   credentials with the newly generated credentials.
 * - After reboot the device will connect with the newly assigned credentials and commence
 *   normal operation.
 *
 * @note If you want to provision the same device a second time, remember to delete the device
 * from _Manage > All devices > Things_.
 * Also note that the device overwrites the claim certificate and keys, so there are several
 * options should you require to re-provision (for example if adding factory reset support):
 * - The application keeps a copy of the claim certificate and keys, then restores them on a
 *   factory reset and deletes the @c aws.thingname key. However, be aware that claim certificates
 *   are intended to be used temporarily and may expire or be revoked. So this method of
 *   re-provisioning may fail if the claim certificate is no longer valid.
 * - The application uses the existing device certificates as claim certificates to re-provision,
 *   provided the device policy includes permissions for the @c create-from-csr and
 *   @c provisioning-templates resources. In our case the @c AllowAll policy allows everything
 *   including these claim permissions. To re-provision, the application simply deletes the
 *   @c aws.thingname key to trigger the provisioning process. This method has the advantage
 *   of requiring less resources by not needing to keep a copy of the claim certificate and
 *   key which saves around 3KB of storage in persistent store.
 */

#include <string.h>
#include "mmhal.h"
#include "mmosal.h"
#include "mmwlan.h"
#include "mmconfig.h"
#include "mm_app_loadconfig.h"
#include "mmipal.h"
#include "mqtt_agent_task.h"
#if defined(ENABLE_SHADOW_APP) && ENABLE_SHADOW_APP
#include "shadow_device_task.h"
#endif
#if defined(ENABLE_OTA_APP) && ENABLE_OTA_APP
#include "ota_update_task.h"
#endif
#if defined(ENABLE_PROVISIONING_APP) && ENABLE_PROVISIONING_APP
#include "fleet_provisioning_task.h"
#endif
#include "sntp_client.h"
#include "core_json.h"
#include "mm_app_common.h"
#include "aws_iot_config.h"

/** Minimum NTP timeout per attempt */
#define NTP_MIN_TIMEOUT             3000
/** We need to back-off at least 60 seconds or most NTP Servers will tell us to go away */
#define NTP_MIN_BACKOFF             60000
/** Minimum back-off jitter per attempt */
#define NTP_MIN_BACKOFF_JITTER      3000
/** Maximum back-off jitter per attempt */
#define NTP_MAX_BACKOFF_JITTER      60000

/**
 * Format string representing a Shadow document with a "reported" state.
 *
 * The real json document will look like this:
 * @code
 * {
 *   "state": {
 *     "reported": {
 *       "powerOn": 1
 *     }
 *   },
 *   "clientToken": "021909"
 * }
 * @endcode
 * Note the client token, which is optional. The token is used to identify the
 * response to an update. The client token must be unique at any given time,
 * but may be reused once the update is completed. For this demo, a timestamp
 * is used for a client token.
 */
#define SHADOW_PUBLISH_JSON               \
    "{"                                   \
        "\"state\":{"                     \
            "\"reported\":{"              \
                "\"powerOn\":%lu"         \
            "}"                           \
        "},"                              \
        "\"clientToken\":\"%06lu\""       \
    "}"

/** Current state of the lamp */
static uint32_t ulCurrentPowerOnState = 0;

/** Desired state of the lamp */
static uint32_t ulDesiredPowerOnState = 0;

/** Semaphore to request change of state of the lamp */
struct mmosal_sem* state_change_sem = NULL;

/**
 * This function is called when an OTA update has been triggered.
 *
 * The primary use case of this function is to clear up space in the file system for the
 * OTA download. Once this function returns there should be enough space available in the
 * file system to store the complete update image.
 * For more information see @c ota_preupdate_cb_fn_t.
 */
void ota_preupdate_callback(void)
{
    /* An OTA update has been triggered, print a message notifying the user */
    printf("An OTA update has been triggered, downloading the file in the background...\n");

    /* Perform any cleanup for the file system such as deleting logs, uploading data, etc.
     * If the device runs out of space in the filesystem while downloading the update then
     * the update will fail. You may block till cleanup is completed. */
}

/**
 * This function is called after an OTA update was processed.
 *
 * The application may perform any logging to note the event and also to migrate any
 * data from the older version if required. The application may also use this callback
 * to restore any files and data it had uploaded to the cloud prior to the update
 * starting. This function may also be used to atomically update firmware and BCF images
 * contained in the update file. For more information see @c ota_postupdate_cb_fn_t.
 *
 * @param update_file Path to the update file - use this to update firmware or BCF if required.
 *                    This file will be automatically deleted on return from this function.
 * @param status      0 on success, bootloader error code on failure.
 */
void ota_postupdate_callback(const char *update_file, int status)
{
    (void) update_file;

    if (status == 0)
    {
        printf("OTA Update completed successfully\n");
    }
    else
    {
        printf("OTA Update failed with error code %d\n", status);
    }
}

/**
 * Callback function invoked on shadow state updates.
 *
 * @param json       The JSON update message, not NULL terminated.
 * @param json_len   The length of the JSON update message.
 * @param status     The status of the update.
 */
void shadow_update_callback(char *json, size_t json_len,
                            enum shadow_update_status status)
{
    /* Make sure the payload is a valid json document. */
    int result = JSON_Validate(json, json_len);

    if (result != JSONSuccess)
    {
        printf("ERR:Invalid JSON document received!\n");
        return;
    }

    static uint32_t ulCurrentVersion = 0; /* Remember the latest version number we've received */
    char * pcOutValue = NULL;
    uint32_t ulOutValueLength = 0UL;
    uint32_t ulVersion = 0;
    uint32_t ulCode = 0;

    switch (status)
    {
    case UPDATE_DELTA:
        /* The json will look similar to this:
         * @code
         * {
         *      "state": {
         *          "powerOn": 1
         *      },
         *      "metadata": {
         *          "powerOn": {
         *              "timestamp": 1595437367
         *          }
         *      },
         *      "timestamp": 1595437367,
         *      "clientToken": "388062",
         *      "version": 12
         * }
         * @endcode
         */

        /* Obtain the version value. */
        result = JSON_Search(json, json_len,
                             "version", strlen("version"),
                             &pcOutValue,
                             (size_t *) &ulOutValueLength);

        if (result != JSONSuccess)
        {
            printf("ERR:Version field not found in JSON document!\n");
            return;
        }
        /* Convert the extracted value to an unsigned integer value. */
        ulVersion = (uint32_t) strtoul(pcOutValue, NULL, 10);
        /* Make sure the version is newer than the last one we received. */
        if (ulVersion <= ulCurrentVersion)
        {
            /* In this demo, we discard the incoming message
             * if the version number is not newer than the latest
             * that we've received before. Your application may use a
             * different approach. */
            printf("ERR:Received unexpected delta update with version %u. "
                   "Current version is %u\n",
                   (unsigned int) ulVersion,
                   (unsigned int) ulCurrentVersion);
            return;
        }
        /* Set received version as the current version. */
        ulCurrentVersion = ulVersion;

        /* Get powerOn state from json documents. */
        result = JSON_Search(json, json_len,
                             "state.powerOn",
                             sizeof("state.powerOn") - 1,
                             &pcOutValue,
                             (size_t *) &ulOutValueLength);

        if (result != JSONSuccess)
        {
            printf("ERR:powerOn field not found in JSON document!\n");
        }
        else
        {
            /* Convert the powerOn state value to an unsigned integer value. */
            ulDesiredPowerOnState = (uint32_t) strtoul(pcOutValue, NULL, 10);

            /* Signal user task about change of state */
            mmosal_sem_give(state_change_sem);
        }
        break;

    case UPDATE_ACCEPTED:
        /* Handle the reported state with state change in /update/accepted topic.
         * Thus we will retrieve the client token from the JSON document to see if
         * it's the same one we sent with reported state on the /update topic.
         * The payload will look similar to this:
         * @code
         *  {
         *      "state": {
         *          "reported": {
         *             "powerOn": 1
         *          }
         *      },
         *      "metadata": {
         *          "reported": {
         *              "powerOn": {
         *                  "timestamp": 1596573647
         *              }
         *          }
         *      },
         *      "version": 14698,
         *      "timestamp": 1596573647,
         *      "clientToken": "022485"
         *  }
         * @endcode
         */

        /* We do not need to do anything here unless we want to implement positive confirmation
         * that our reported state was received and acted on by the server. In which case ensure
         * you check that the @c clientToken matches the one we sent in the report. */
        break;

    case UPDATE_REJECTED:
        /* The payload will look similar to this:
         * {
         *    "code": error-code,
         *    "message": "error-message",
         *    "timestamp": timestamp,
         *    "clientToken": "token"
         * }
         */

        /*  Obtain the error code. */
        result = JSON_Search(json, json_len,
                             "code",
                             sizeof("code") - 1,
                             &pcOutValue,
                             (size_t *) &ulOutValueLength);

        /* Convert the code to an unsigned integer value. */
        ulCode = (uint32_t) strtoul(pcOutValue, NULL, 10);

        printf("ERR:Received rejected response code %lu\n",
                ulCode);

        /*  Obtain the message string. */
        result = JSON_Search(json, json_len,
                             "message",
                             sizeof("message") - 1,
                             &pcOutValue,
                             (size_t *) &ulOutValueLength);
        printf("    Message: %.*s\n", (int)ulOutValueLength, pcOutValue);

        break;
    }
}

/**
 * Main shadow loop.
 *
 * @param shadow_name Name of the shadow.
 */
void aws_shadow_loop(char *shadow_name)
{
    for (;;)
    {
        mmosal_sem_wait(state_change_sem, UINT32_MAX);

        if (ulDesiredPowerOnState == 1)
        {
            /* Set the new powerOn state. */
            printf("INF:Setting powerOn state to 1.\n");
            mmhal_set_led(LED_BLUE, LED_ON);
            ulCurrentPowerOnState = ulDesiredPowerOnState;
        }
        else if (ulDesiredPowerOnState == 0)
        {
            /* Set the new powerOn state. */
            printf("INF:Setting powerOn state to 0.\n");
            mmhal_set_led(LED_BLUE, LED_OFF);
            ulCurrentPowerOnState = ulDesiredPowerOnState;
        }
        else
        {
            /* Set the new powerOn state. */
            printf("ERR:Invalid power state %lu requested.\n", ulDesiredPowerOnState);
        }

        char UpdateDocument[MAX_JSON_LEN];
        printf("INF:Reporting change in PowerOn state to %lu.\n", ulCurrentPowerOnState);

        /* Create a new client token and save it for use in the callbacks */
        uint32_t ulClientToken = (mmosal_get_time_ticks() % 1000000);

        /* Generate update report. */
        (void) memset(UpdateDocument, 0x00, sizeof(UpdateDocument));
        snprintf(UpdateDocument, MAX_JSON_LEN, SHADOW_PUBLISH_JSON,
                    ulCurrentPowerOnState, ulClientToken);

        /* Send update. */
        aws_publish_shadow(shadow_name, UpdateDocument);

        printf("INF:Publishing to /update with following client token %lu.\n", ulClientToken);
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_init(void)
{
    printf("\n\nMorse AWS IoT Demo (Built " __DATE__ " " __TIME__ ")\n\n");

    /* Initialize and connect to Wi-Fi, blocks till connected */
    app_wlan_init();
    app_wlan_start();

    /* Wi-Fi is connected, sync to NTP - required for certificate expiry validation */
    char sntp_server[64];
    strncpy(sntp_server, "0.pool.ntp.org", sizeof(sntp_server)); /* default if key is not found */
    mmconfig_read_string("sntp.server", sntp_server, sizeof(sntp_server));
    sntp_sync_with_backoff(sntp_server, NTP_MIN_TIMEOUT, NTP_MIN_BACKOFF,
                           NTP_MIN_BACKOFF_JITTER, NTP_MAX_BACKOFF_JITTER, UINT32_MAX);

    /* Display current time */
    time_t now;
    now = mmhal_get_time();
    printf("Current Time (UTC) is : %s\r\n", ctime(&now));

    /* First spool up the MQTT agent task */
    start_mqtt_agent_task();

    /* Look for shadow name in config store, if none found use classic shadow (NULL) */
    char *shadow_name = NULL;
    mmconfig_alloc_and_load(AWS_KEY_SHADOW_NAME, (void**) &shadow_name);

    state_change_sem = mmosal_sem_create(1, 1, "state_change_sem");

    /* Check if fleet provisioning is needed */
    if (mmconfig_read_string(AWS_KEY_THING_NAME, NULL, 0) < 0)
    {
#if defined(ENABLE_PROVISIONING_APP) && ENABLE_PROVISIONING_APP
        /* Ensure provisioning template is set */
        if (mmconfig_read_string(AWS_KEY_PROVISIONING_TEMPLATE, NULL, 0) > 0)
        {
            /* Device not registered, so start fleet provisioning, returns only on failure. */
            printf("Initiating fleet provisioning...\n");
            do_fleet_provisioning();
            printf("Failed to provision device, unable to continue.\n"
                   "Please see getting started guide on how to provision.\n");
            return;
        }
#else
        printf("Device is not provisioned, "
            "please see getting started guide on how to provision.\n");
#endif
    }

#if defined(ENABLE_OTA_APP) && ENABLE_OTA_APP
    /* Now spool up the OTA task */
    start_ota_update_task(ota_preupdate_callback, ota_postupdate_callback);
#endif

#if defined(ENABLE_SHADOW_APP) && ENABLE_SHADOW_APP
    /* Start the device shadow processing loop */
    aws_create_shadow(shadow_name, shadow_update_callback);
    aws_shadow_loop(shadow_name);
#endif
}
