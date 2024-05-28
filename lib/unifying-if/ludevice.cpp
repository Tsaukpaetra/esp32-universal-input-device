/*
 Copyright (C) 2017 Ronan Gaillard <ronan.gaillard@live.fr>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.
*/
#include "ludevice.h"

ludevice::ludevice() : ludevice(3, 2)
{
}

ludevice::ludevice(uint8_t _cepin, uint8_t _cspin) : radio(_cepin, _cspin)
{
}

void ludevice::setAddress(uint64_t address)
{
    setAddress((uint8_t *)&address);
}

void ludevice::setAddress(uint8_t *address)
{
    uint8_t address_net[5];

    address_net[0] = address[4];
    address_net[1] = address[3];
    address_net[2] = address[2];
    address_net[3] = address[1];
    address_net[4] = address[0];
    //printf("Setting address: %s\r\n", hexs(address_net, 5));

    

    radio.stopListening();
    //Pipe 2 only pays attention to the first character, and we want it to be 0
    radio.openReadingPipe(1, address_net); //Listen for things specifically for us
    radio.openReadingPipe(2, *"");  //Listen for the dongle
    radio.openWritingPipe(address_net);
}

bool ludevice::begin()
{
    if (Config == nullptr)
        Config = new luconfig();

    printConfig();

    uint8_t init_status = radio.begin();

    if (RadioDebug)
        radio.printDetails();

    if (init_status == 0 || init_status == 0xff)
    {
        Status = luStatus::Disabled;
        return false;
    }

    // aes_base = 0x5897AF67;
    // aes_base = 0x5897AF60;
    aes_base = random(0xfffffff + 1) << 4;

    radio.stopListening();
    if (1)
    {
        radio.setAutoAck(1);
        radio.setRetries(3, 1);
        radio.setPayloadSize(22);
        radio.enableDynamicPayloads();
        radio.enableAckPayload();
        radio.enableDynamicAck();
        // radio.openWritingPipe(PAIRING_MAC_ADDRESS);
        // radio.openReadingPipe(1, PAIRING_MAC_ADDRESS);
        radio.setChannel(Config->current_channel);
        radio.setDataRate(RF24_2MBPS);
        // UNKNOWN WHAT THIS DOES?
        // {
        //     // writeRegister(SETUP_AW, 0x03); // Reset addr size to 5 bytes
        //     digitalWrite(DEFAULT_CS_PIN, LOW);
        //     SPI.transfer(W_REGISTER | (REGISTER_MASK & 0x3));
        //     SPI.transfer(0x03);
        //     digitalWrite(DEFAULT_CS_PIN, HIGH);
        // }
    }
    radio.stopListening();


    radio.stopListening();
    send_alive_timer = 0;

    Status = luStatus::Disconnected;
    return true;
}

void ludevice::printConfig()
{
    printf("Config - CE: %d ,CS: %d\r\n", Config->PIN_CE, Config->PIN_CS);
    printf("Config - RF Address:   %s\r\n", hexs(Config->rf_address, 5));
    printf("Config - Device Key Derived: %s\r\n", hexs(Config->device_key, 16));
    printf("Config - CHANNEL:            %d\r\n", Config->current_channel);

}

void ludevice::setChecksum(uint8_t *payload, uint8_t len)
{
    uint8_t checksum = 0;

    for (uint8_t i = 0; i < (len - 1); i++)
        checksum += payload[i];

    payload[len - 1] = -checksum;
}

void ludevice::hidpp10(uint8_t *rf_payload, uint8_t payload_size)
{
    uint8_t rf_response[22] = {0};
    uint8_t reply = 22;
    const char *name = "UNKNOWN PACKET!!! PLEASE TEST AT DONGLE SIDE!!!";

    rf_response[0] = rf_payload[0];
    rf_response[1] = 0x40 | rf_payload[1]; // report ID
    rf_response[2] = rf_payload[2];        // device index
    rf_response[3] = rf_payload[3];        // sub id
    rf_response[4] = rf_payload[4];        // address

    uint32_t feature_id = (rf_payload[5] << 8) + (rf_payload[6]);
    uint32_t addr_param = (rf_payload[4] << 8) + (rf_payload[5]);

    // request sub_id is [2 + 1]
    switch (rf_payload[3])
    {
    case 0x80: // SET_REGISTER
        // reply = 10;
        printf("SETTING REGISTER!!!!!!!!!");
        break;
    case 0x81: // GET_REGISTER
        reply = 10;
        switch (addr_param)
        {
        case 0xf101:
            // firmware major
            name = "Firmware Major";
            rf_response[5] = rf_payload[5];
            rf_response[6] = (firmware_version >> 24) & 0xff;
            rf_response[7] = (firmware_version >> 16) & 0xff;
            break;
        case 0xf102:
            name = "Firmware Minor";
            rf_response[5] = rf_payload[5];
            rf_response[6] = (firmware_version >> 8) & 0xff;
            rf_response[7] = (firmware_version >> 0) & 0xff;
            break;
        case 0xf103:
            name = "Firmware 0x03";
            rf_response[5] = rf_payload[5];
            rf_response[6] = 0x01;
            rf_response[7] = 0x02;
            break;
        case 0xf104:
            name = "Firmware 0x04";
            rf_response[5] = rf_payload[5];
            rf_response[6] = 0x02;
            rf_response[7] = 0x14;
            break;
        case 0x700:
            name = "HIDPP_REG_BATTERY_STATUS bad";
            rf_response[5] = 0x7; // rf_payload[5];
            rf_response[6] = 3;   // capacity
            rf_response[7] = 0x0; // level 1-7
            rf_response[8] = 0x0; // discharing
            break;
        case 0xd00:
            // reply = 0;
            name = "HIDPP_REG_BATTERY_MILEAGE";
            rf_response[5] = 50;     // capacity: 0 - 100
            rf_response[6] = 0;      // nothing: 0
            rf_response[7] = 0 << 6; // status: 0 - discharging, 1 - charging
            break;
        default:
            reply = 0;
            name = "UNKNOWN";
            break;
        }
        break;
    default:
        switch (feature_id)
        {
        case 0x03:
            // 00 10 9E 00 02 00 03 00 B6 97
            // 00 51 9E 00 02 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0D - response
            if (addr_param = 0x200)
            {
                name = "copied from K270 (2)";
                reply = 22;
                rf_response[3] = 0x02;
                rf_response[4] = 0x02;
                rf_response[5] = 0x00;
            }
            break;
        case 0x3f13:
            // 00 10 0E 00 12 3F 13 00 00 7E
            // 00 51 0E 00 12 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 8D - response
            name = "copied from K270 (1)";
            reply = 22;
            rf_response[3] = 0x00;
            rf_response[4] = 0x12;
            rf_response[5] = 0x02;
            break;
        case 0x0000: // 00 10 0E 00 12 00 00 00 B6 1A
            name = "we don't understand HID++ 2.0";
            reply = 10;
            // HID++ 1.0
            rf_response[3] = 0x8f;
            rf_response[4] = 0;
            // RF rf_response Results start from [4 + 1]
            rf_response[5] = 0x10 + (rf_payload[4] & 0xf);
            rf_response[6] = 1;
            rf_response[7] = 0;
            rf_response[8] = 0;
        }
        break;
    }

    if (reply && (rf_payload[1] == 0x10 || rf_payload[1] == 0x11))
    {
        if (reply == 10)
            rf_response[1] = 0x50;
        if (reply == 22)
            rf_response[1] = 0x51;
        radiowrite(rf_response, reply, name, 5);
    }
}

void ludevice::hidpp20(uint8_t *rf_payload, uint8_t payload_size)
{
    // https://initrd.net/stuff/mousejack/doc/pdf/DEFCON-24-Marc-Newlin-MouseJack-Injecting-Keystrokes-Into-Wireless-Mice.slides.pdf
    // https://drive.google.com/file/d/0B4Pb6jGAmjoKQ3hlZDFxUHVqRkU/view

    // [16.922] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [16.923] 9D:65:CB:58:4D // ACK
    // [17.015] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [17.015] 9D:65:CB:58:4D // ACK
    // [17.108] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [17.108] 9D:65:CB:58:4D // ACK
    // [17.201] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [17.201] 9D:65:CB:58:4D // ACK
    // [17.294] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [17.294] 9D:65:CB:58:4D 00:10:4D:00:14:00:00:00:00:8F // ACK payload; requesting HID++ version
    //                         00:10:ce:00:12:3f:13:00:00:be
    //                         00:10:ce:00:12:00:00:00:11:ff
    // [17.302] 9D:65:CB:58:4D 00:51:4D:00:14:04:05:0000000000000000000000000000:45 // response (HID++ 4.5)
    // [17.302] 9D:65:CB:58:4D // ACK
    // [17.387] 9D:65:CB:58:4D 0040006E52 // keepalive, 110ms interval
    // [17.387] 9D:65:CB:58:4D // ACK
    // https://lekensteyn.nl/files/logitech/logitech_hidpp_2.0_specification_draft_2012-06-04.pdf
    // https://github.com/mame82/UnifyingVulnsDisclosureRepo/blob/master/talk/phishbot_2019_redacted3.pdf
    // https://raw.githubusercontent.com/torvalds/linux/master/drivers/hid/hid-logitech-hidpp.c

    // [0] 00 - device index
    // [1] 10 - Report ID
    //          0x10, 7 bytes UNIFYING_RF_REPORT_HIDPP_SHORT, 0x0E = UNIFYING_RF_REPORT_LED
    //          0x11, 20 bytes REPORT_ID_HIDPP_VERY_LONG
    // [2] CE - Device Index / RF prefix
    // [3] 00 - Sub ID
    // ---
    // [4] 12 - Address
    // [5] xx - value 0
    // [6] xx - value 1
    // [7] xx - value 2
    // [8] 00 -
    // [9] xx - checksum

    uint8_t rf_response[22] = {0};
    uint8_t reply = 22;
    const char *name;

    rf_response[0] = rf_payload[0];
    rf_response[1] = 0x51;
    rf_response[2] = rf_payload[2];
    rf_response[3] = rf_payload[3];
    rf_response[4] = rf_payload[4];

    uint32_t feature_id = (rf_payload[5] << 8) + (rf_payload[6]);
    // https://lekensteyn.nl/files/logitech/logitech_hidpp_2.0_specification_draft_2012-06-04.pdf
    switch (feature_id)
    {
    default:
        reply = 0;
        break;
    case 0x0000: // root
        name = "root 0x0000";
        reply = 10;
        // HID++ 2.0, 4.5
        // rf_response[3] = 0x0;
        // rf_response[4] = 0x10 + (ack_payload[4] & 0xf);
        // RF rf_response Results start from [4 + 1]
        rf_response[5] = 0x2;
        rf_response[6] = 0x0;
        rf_response[7] = rf_payload[8]; // ping
        rf_response[8] = 0;
        break;
    case 0x0003: // device info

        // request parms starts from ack_payload[4]
        name = "firmware 0x0003";
        // RF rf_response Results start from [4 + 1]
        rf_response[5] = 0x0;
        rf_response[6] = 'a';
        rf_response[7] = 'b';
        rf_response[8] = 'c';
        rf_response[9] = 0x33;
        rf_response[10] = 0x44;
        rf_response[11] = 0x1;
        rf_response[12] = 0x1;
        rf_response[13] = 0x0; // xx
        rf_response[14] = 'K';
        rf_response[15] = 'S';
        rf_response[16] = 'B';

        break;
    case 0x1000: // battery
        name = "battery 0x1000";
        // RF rf_response Results start from [4 + 1]
        rf_response[5] = 80; // BatteryDischargeLevel
        rf_response[6] = 70; // BatteryDischargeNextLevel
        rf_response[7] = 2;  // 0 - charging
        break;
    case 0x1d4b: // wireless device status
        name = "wireless 0x1d4b";
        rf_response[5] = 0;
        rf_response[6] = 0;
        rf_response[7] = 0;
        break;
    }
    if (reply && rf_payload[1] == 0x10)
    {
        if (reply == 10)
            rf_response[1] = 0x50;
        if (reply == 22)
            rf_response[1] = 0x51;
        radiowrite(rf_response, reply, name, 1);
    }
}

void ludevice::loop(void)
{
    if (!(Status & luStatus::Connected) && backoff_timer > 400)
    {
        // Try to connect if we can
        if (!reconnect())
        {
            if (send_alive_timer < AutoPairTimeout)
                pair();
            else
                Serial.println("Not auto");
        }
        backoff_timer = 0;
    }

    if (!connected())
        return;

    uint8_t response_size = 0;
    uint8_t *rf_payload;

    while (radio.available())
    {
        response_size = read(rf_payload);
        hidpp10(rf_payload, response_size);
        // hidpp20(rf_payload, response_size);
    }
    stay_alive();
}

void ludevice::stay_alive(void)
{
    if (send_alive_timer > keep_alive_send_trigger)
    {
        if(keep_alive_ticks++>keep_alive_stage_ticks[stay_alive_stage] 
        && keep_alive_stage_ticks[stay_alive_stage] > 0)
        {
            //Try to move to the next level
            update_keep_alive(stay_alive_stage+1);
            keep_alive_ticks = 0;
        } else {
            char buffer[40];
            unsigned long t = idle_timer;
            sprintf(buffer, "%dms keep alive (idle: %d)", keep_alive_stages[stay_alive_stage], t);
            if(!radiowrite_ex(keep_alive_packet, sizeof(keep_alive_packet), buffer, 5))
            {
                if(++keep_alive_miss > 25)
                {
                    //Failed to keep alive, we dead!
                    Status = (luStatus)(Status & ~luStatus::Connected);
                    SendStatusUpdate();
                }
            } else {
               keep_alive_miss = 0; 
            }
        }
        send_alive_timer = 0;
    }
}

bool ludevice::update_keep_alive(uint8_t stage)
{
    send_alive_timer = 0;
    if(stage == stay_alive_stage) 
        return true;
    char buffer[30];

    uint8_t keep_alive_change_packet[10];
    constructPacket(keep_alive_change_packet,10,(uint8_t[]){ 
            0, //An unknown flag maybe?
            (uint8_t)((keep_alive_stages[stage] & 0xff00) >> 8),
            (uint8_t)(keep_alive_stages[stage] & 0x00ff)
        },
        LOGITACKER_DEVICE_REPORT_TYPES_SET_KEEP_ALIVE | LOGITACKER_DEVICE_REPORT_TYPES_KEEP_ALIVE
    );

    sprintf(buffer, "set keep alive to %d ms", keep_alive_stages[stage]);
    if (radiowrite_ex(keep_alive_change_packet, sizeof(keep_alive_change_packet), buffer, 5))
    {
        stay_alive_stage = stage;
        
        constructPacket(keep_alive_packet,5,(uint8_t[])
            { 
                (uint8_t)(keep_alive_stages[stage] & 0xff00) >> 8,
                (uint8_t)(keep_alive_stages[stage] & 0x00ff)
            },
            LOGITACKER_DEVICE_REPORT_TYPES_SET_KEEP_ALIVE | LOGITACKER_DEVICE_REPORT_TYPES_KEEP_ALIVE
        );
        keep_alive_send_trigger = keep_alive_stages[stage] * 0.8f;
        return true;
    }
    return false;
}

int ludevice::pair()
{
    uint8_t device_raw_key_material[16];
    bool passed;
    uint8_t retry = 10;
    uint8_t bis_retry;
    uint8_t response_size;
    uint8_t *response;
    uint8_t pairing_packet[22];
    uint8_t pairing_packet_bis[5];
    uint8_t prefix;


    Status = luStatus::Connecting;
    SendStatusUpdate();
    setAddress(PAIRING_MAC_ADDRESS);
    
    {
        //Create the first pairing packet
        constructPacket(pairing_packet,22, (uint8_t[]){
            1, //Pair step 1
            Config->rf_address[0],
            Config->rf_address[1],
            Config->rf_address[2],
            Config->rf_address[3],
            Config->rf_address[4],
            0x14,                    // default keep_alive
            (uint8_t)((device_wpid & 0xff00) >> 8), // wireless PID MSB
            (uint8_t)((device_wpid & 0x00ff) >> 0), // wireless PID LSB
            protocol, 0x00,
            device_type, caps,
            0x00, 0x00, 0x00, 0x00, 0x00,
            pp1_unknown, //Special value, sets modes for the transmission?
        },LOGITACKER_DEVICE_REPORT_TYPES_PAIRING,PAIRING_MARKER_PHASE_1);
        constructPacket(pairing_packet_bis,5,(uint8_t[]){
            1, //Pair step 1
            Config->rf_address[0]
        }, 0, PAIRING_MARKER_PHASE_1);

        memcpy(device_raw_key_material, Config->rf_address, 4);       // REQ1 device_rf_address
        memcpy(device_raw_key_material + 4, (uint8_t*)&device_wpid, 2); // REQ1 device_wpid
        
        if(!pair_radiotransact("REQ1", pairing_packet, "BIS1", pairing_packet_bis, response, response_size, 11, true))
        {
            return false;
        }

        // extract info from BIS1 response
        {
            memcpy(device_raw_key_material + 6, response + LOGITACKER_UNIFYING_PAIRING_RSP1_OFFSET_DONGLE_WPID, 2); // RSP1 dongle_wpid
            for (int i = 0; i < 5; i++)
                Config->rf_address[i] = response[(3 + i)];
            setAddress(Config->rf_address);
        }
    }
    
    {
        uint8_t nonce[4];
        uint8_t serial[4];
        for (int x = 0; x < 4; x++)
        {
            // Random bytes for the serial and nonce
            nonce[x] = random(0xff);
            serial[x] = random(0xff);
        }

        //Create the second pairing packet
        constructPacket(pairing_packet,22, (uint8_t[]){
            2, //Pair step 2
            nonce[0],                                                 // device nonce MSB
            nonce[1],                                                 // device nonce
            nonce[2],                                                  // device nonce
            nonce[3],                                                  // device nonce LSB
            serial[0],                                                // device serial MSB
            serial[1],                                                // device serial
            serial[2],                                                 // device serial
            serial[3],                                                 // device serial LSB
            (uint8_t)((report_types & 0x000000ff) >> 0),                                           // device report types
            (uint8_t)((report_types & 0x0000ff00) >> 8),                                           // device report types
            (uint8_t)((report_types & 0x00ff0000) >> 16),                                          // device report types
            (uint8_t)((report_types & 0xff000000) >> 24),                                          // device report types
            LOGITACKER_DEVICE_USABILITY_INFO_PS_LOCATION_ON_THE_EDGE_OF_TOP_RIGHT_CORNER, // device_usability_info
        },LOGITACKER_DEVICE_REPORT_TYPES_PAIRING,PAIRING_MARKER_PHASE_2);
        constructPacket(pairing_packet_bis,5,(uint8_t[]){
            2, //Pair step 2
            nonce[0]
        }, 0, PAIRING_MARKER_PHASE_2);
        //Store persistent info into the key blob
        memcpy(device_raw_key_material + 8, nonce, 4); // REQ2 device_nonce


        if(!pair_radiotransact("REQ2", pairing_packet, "BIS2", pairing_packet_bis, response, response_size, retry))
        {
            return false;
        }

        // extract info from BIS2 response
        memcpy(device_raw_key_material + 12, response + LOGITACKER_UNIFYING_PAIRING_RSP2_OFFSET_DONGLE_NONCE, 4); // RSP2 dongle_nonce
    }
    
    {

        //Create the third pairing packet
        constructPacket(pairing_packet,22, (uint8_t[]){
            3, //Pair step 3
            1, //number of reports fixed to 1
        },LOGITACKER_DEVICE_REPORT_TYPES_PAIRING,PAIRING_MARKER_PHASE_3);
        constructPacket(pairing_packet_bis,5,(uint8_t[]){
            3, //Pair step 3
            1, //Something...
        }, 0, PAIRING_MARKER_PHASE_3);

        //Inject the device name and ensure it does not overflow
        uint8_t devNameLength = strlen(device_name);
        if(devNameLength > 14)
            devNameLength = 14;
        pairing_packet[4] = devNameLength;
        memcpy(pairing_packet + 5, device_name,devNameLength);
        //Recalculate the checksum
        setChecksum(pairing_packet,22);

        if(!pair_radiotransact("REQ3", pairing_packet, "BIS3", pairing_packet_bis, response, response_size, retry))
        {
            return false;
        }
    }
    
    {
        //Create the final pairing packet
        constructPacket(pairing_packet,10, (uint8_t[]){
            0x06, //Pair step final
            0x01, //Something... Flag to say we're done sending info?
        },LOGITACKER_DEVICE_REPORT_TYPES_SET_KEEP_ALIVE);
        if (!radiowrite(pairing_packet, 10, "Final", retry))
        {
            return false;
        }
    }
    
    /* Save address to config */
    Config->device_key[2] = device_raw_key_material[0];
    Config->device_key[1] = device_raw_key_material[1] ^ 0xFF;
    Config->device_key[5] = device_raw_key_material[2] ^ 0xFF;
    Config->device_key[3] = device_raw_key_material[3];
    Config->device_key[14] = device_raw_key_material[4];
    Config->device_key[11] = device_raw_key_material[5];
    Config->device_key[9] = device_raw_key_material[6];
    Config->device_key[0] = device_raw_key_material[7];
    Config->device_key[8] = device_raw_key_material[8];
    Config->device_key[6] = device_raw_key_material[9] ^ 0x55;
    Config->device_key[4] = device_raw_key_material[10];
    Config->device_key[15] = device_raw_key_material[11];
    Config->device_key[10] = device_raw_key_material[12] ^ 0xFF;
    Config->device_key[12] = device_raw_key_material[13];
    Config->device_key[7] = device_raw_key_material[14];
    Config->device_key[13] = device_raw_key_material[15] ^ 0x55;

    printConfig();
    SendConfigUpdated();

    AES_init_ctx(&ctx, Config->device_key);
    Status = luStatus::Connected;

    // //Connect with our new parameters
    return register_device(false);
    // return true;
}

uint8_t ludevice::read(uint8_t *&packet)
{
    uint8_t packet_size = 22;

    if (radio.available())
    {
        packet = _read_buffer;
        radio.read(packet, packet_size);
        if (1)
        {
            if ((packet[19] == packet[20]) && (packet[20] == packet[21]))
                packet_size = 10;
            if (packet_size == 10 && ((packet[7] == packet[8]) && (packet[8] == packet[9])))
                packet_size = 5;
        }
        if (RadioDebug && packet[1] != 0xe)
        {
            // printf("IN [%2d]: %2d                   ", packet_size, current_channel);
            printf("IN [%2d]:                  %2d   ", packet_size, Config->current_channel);
            printf("%s\r\n", hexs(packet, packet_size));
        }
        return packet_size;
    }
    return 0;
}

bool ludevice::radiowrite(uint8_t *packet, uint8_t packet_size, const char *name, uint8_t retry)
{
    if (!radiowrite_ex(packet, packet_size, name, retry))
    {
        return false;
    }
    //Reset the keep_alive timer and make sure we're using the highest rate
    return true && update_keep_alive(0);
}

bool ludevice::radiowrite_ex(uint8_t *packet, uint8_t packet_size, const char *name, uint8_t retry, bool cycleChannel)
{
    char outcome;

    // retry = 1;
    outcome = '!';
    while (retry)
    {
        //setChecksum(packet, packet_size);
        if (radio.write(packet, packet_size))
            outcome = ' ';
        else
            retry--;

        if (RadioDebug)
        {
            // printf("OUT[%2d]: %2d  %s %c ", packet_size, current_channel, hexs(rf_address, 5), outcome);
            // printf("%d OUT[%2d]:  %s  %2d %c ", millis(), packet_size, hexs(rf_address, 5), current_channel, outcome);
            printf("OUT[%2d]:  %s  %2d %c ", packet_size, hexs(Config->rf_address, 5), Config->current_channel, outcome);
            printf("%s", hexs(packet, packet_size));
            if (name != NULL)
                printf(" - %s\r\n", name);
            else
                printf("\r\n");
        }

        if (outcome == '!')
        {
            if (cycleChannel)
            {
                changeChannel();
            }
        }
        else
            break;
    };

    if (outcome == '!')
        return false;

    return true;
}

bool ludevice::pair_radiotransact(const char *name, uint8_t *packet, const char *name_bis,  uint8_t *packet_bis, uint8_t *&response, uint8_t &response_size, uint8_t retry, bool cycleChannel)
{
    //Pairing packets are always 22 bytes
    if (!radiowrite_ex(packet, 22, name, retry, cycleChannel))
    {
        // Big fail!
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        return false;    
    }
    
    uint8_t bis_retry = 10;
    while (bis_retry)
    {
        //pairing_bis packets are always 5 bytes
        if (radiowrite_ex(packet_bis, 5, name_bis, 1))
        {
            response_size = read(response);
            if (response_size > 0)
            {
                if (response[0] != packet_bis[0])
                {
                    printf("Wrong prefix\r\n");
                }
                else
                    break;
            }
            else
                printf("Empty response\r\n");
        }
        bis_retry--;
    }
    if (bis_retry == 0)
    {
        // Big fail!
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        return false;
    }
    return true;
}
template <std::size_t payload_size>
void ludevice::constructPacket(uint8_t *packet, uint8_t packet_size, const uint8_t (&payload)[payload_size], uint8_t reportType, uint8_t pairStage)
{
    //Set the pair phase and reportType
    packet[0] = pairStage;
    //Always add the keep alive flag since we're the device connecting to the dongle
    packet[1] = reportType | LOGITACKER_DEVICE_REPORT_TYPES_KEEP_ALIVE;
    //Toss in the payload
    memcpy(packet + 2, payload,payload_size);

    //clear it out any remaining (the last byte will be set in the checksum)
    memset(packet + 2 + payload_size, 0, packet_size - payload_size - 3);
    //Set up the checksum
    setChecksum(packet,packet_size);

}

void ludevice::changeChannel()
{
    channel_ix = ++channel_ix % (Status & luStatus::UseSaved ? 25 : 11);
    Config->current_channel = Status & luStatus::UseSaved ? channel_tx[channel_ix] : channel_pairing[channel_ix];

    radio.setChannel(Config->current_channel);
}

bool ludevice::reconnect()
{
    return register_device();
}

bool ludevice::register_device(bool SwitchToDongleAddress)
{
    uint8_t prefix;
    uint8_t *response;
    bool failed;
    uint8_t registration[22];


    Status = (luStatus)(Status | luStatus::UseSaved | luStatus::Connecting);
    SendStatusUpdate();

    if(SwitchToDongleAddress)
    {
        uint8_t piece = Config->rf_address[4];
        Config->rf_address[4] = 0;
        setAddress(Config->rf_address);
        Config->rf_address[4] = piece;
    }

    constructPacket(registration, 22, (uint8_t[]){
        Config->rf_address[4],
        //Magic!
        0x07, 0x00, 0x01, 0x01,0x01
    },LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP,Config->rf_address[4]);

    if (!radiowrite_ex(registration, 22, "register1", 5))
    {        
        // Big fail!
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        
        if(SwitchToDongleAddress)
        {
            //Switch back to the registered one
            setAddress(Config->rf_address);
        }
        return false;    
    }

    constructPacket(registration, 10, (uint8_t[]){
        //Magic!
        0x07
    },LOGITACKER_DEVICE_REPORT_TYPES_SET_KEEP_ALIVE,Config->rf_address[4]);
    if (!radiowrite_ex(registration, 10, "register2", 5))
    {
        // Big fail!
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        
        if(SwitchToDongleAddress)
        {
            //Switch back to the registered one
            setAddress(Config->rf_address);
        }
        return false;
    
    }

    if(SwitchToDongleAddress)
    {
        //Switch back to the registered one
        setAddress(Config->rf_address);
    }

    constructPacket(registration,22, (uint8_t[]){
        Config->rf_address[4],
        //Magic!
        0x04, 0x00, 0x46, 0x14
    },LOGITACKER_DEVICE_REPORT_TYPES_LONG_HIDPP);
    if (!radiowrite_ex(registration, 22, "hello", 5))
    {
        // Big fail!
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        return false;
    }

    if (!update_keep_alive(0))
    {
        // Big fail in handshake
        Status = (luStatus)(Status & ~luStatus::Connecting);
        SendStatusUpdate();
        return false;
    }

    Status = (luStatus)(luStatus::Connected | luStatus::UseSaved);
    SendStatusUpdate();
    AES_init_ctx(&ctx, Config->device_key);
    return true;
}

void ludevice::move(uint16_t x_move, uint16_t y_move, uint8_t scroll_v, uint8_t scroll_h, bool leftClick, bool rightClick)
{
    idle_timer = 0;

    uint8_t mouse_payload[10];// = {0x00, 0xC2, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    uint32_t cursor_velocity = ((uint32_t)y_move & 0xFFF) << 12 | (x_move & 0xFFF);

    constructPacket(mouse_payload,10, (uint8_t[]){
        (uint8_t)(leftClick | (rightClick << 1)),
        0, //Magic?
        (uint8_t)((cursor_velocity & 0x000000ff) >> 0),  
        (uint8_t)((cursor_velocity & 0x0000ff00) >> 8),  
        (uint8_t)((cursor_velocity & 0x00ff0000) >> 16), 
        scroll_v,
        scroll_h
    },LOGITACKER_DEVICE_REPORT_TYPES_MOUSE | 0x80);

    radiowrite(mouse_payload,10,"mouse move");
}

void ludevice::move(uint16_t x_move, uint16_t y_move)
{
    move(x_move, y_move, false, false);
}

void ludevice::move(uint16_t x_move, uint16_t y_move, bool leftClick, bool rightClick)
{
    move(x_move, y_move, 0, 0, leftClick, rightClick);
}

void ludevice::move(uint16_t x_move, uint16_t y_move, uint8_t scroll_v, uint8_t scroll_h)
{
    move(x_move, y_move, scroll_v, scroll_h, false, false);
}

void ludevice::click(bool leftClick, bool rightClick)
{
    move(0, 0, leftClick, rightClick);
}

void ludevice::scroll(uint8_t scroll_v, uint8_t scroll_h)
{
    move(0, 0, scroll_v, scroll_h, false, false);
}

void ludevice::scroll(uint8_t scroll_v)
{
    scroll(scroll_v, 0);
}

void ludevice::wipe_pairing(void)
{
    memset(Config->rf_address, 0, 5);
    memset(Config->device_key, 0, 5);
}

char *ludevice::hexs_ex(uint8_t *x, uint8_t length, bool reverse, char separator)
{
    uint8_t c;
    _hexs[0] = 0;
    for (int i = 0; i < length; i++)
    {
        c = x[i];
        if (reverse)
            c = x[length - 1 - i];
        sprintf(_hexs + (i * 3), "%02X", c);
        if (i < length - 1)
            sprintf(_hexs + (i * 3) + 2, "%c", separator);
    }

    return _hexs;
}

char *ludevice::hexa(uint8_t *x, uint8_t length)
{
    return hexs_ex(x, length, true, ':');
}

char *ludevice::hexs(uint8_t *x, uint8_t length)
{
    return hexs_ex(x, length, false, ' ');
}

void ludevice::typep(uint8_t scan1, uint8_t scan2, uint8_t scan3, uint8_t scan4, uint8_t scan5, uint8_t scan6)
{
    idle_timer = 0;

    uint8_t key_payload[] = {
        0x00,
        LOGITACKER_DEVICE_REPORT_TYPES_KEYBOARD | LOGITACKER_DEVICE_REPORT_TYPES_KEEP_ALIVE | 0x80,
        0x00, // [2] modifier
        0x00,
        0x00, // [4] scancode
        0x00, 0x00, 0x00, 0x00,
        0x00};

    key_payload[3] = 0x4;  // send 'a'
    key_payload[4] = 0x37; // send '.'
    key_payload[3] = scan1;
    key_payload[4] = scan2;
    key_payload[5] = scan3;
    key_payload[6] = scan4;
    key_payload[7] = scan5;
    key_payload[8] = scan6;
    setChecksum(key_payload, 10);
    radiowrite(key_payload, 10, "plain key", 5);
}

void ludevice::typem(uint16_t scan1, uint16_t scan2)
{
    uint8_t key_payload[10];
    
    constructPacket(key_payload,10, (uint8_t[]){
        ((scan1 & 0x00ff) >> 0),
        ((scan1 & 0xff00) >> 8),
        ((scan2 & 0x00ff) >> 0),
        ((scan2 & 0xff00) >> 8),
    },LOGITACKER_DEVICE_REPORT_TYPES_MULTIMEDIA | 0x80);

    // PAGE UP (0x4B)
    // PAGE DOWN (0x4E)
    // ESC (0x29)
    // F5 (0x3E)
    // PERIOD (0x37)
    // B (0x05)
    radiowrite(key_payload, 10, "media key", 5);
}

void ludevice::typee(uint8_t scan1, uint8_t scan2, uint8_t scan3, uint8_t scan4, uint8_t scan5, uint8_t scan6)
{
    uint32_t temp_counter;
    bool ret;

    uint8_t rf_frame[22] = {0};
    uint8_t plain_payload[8] = {0};

    idle_timer = 0;

    plain_payload[1] = scan1;
    plain_payload[2] = scan2;
    plain_payload[3] = scan3;
    plain_payload[4] = scan4;
    plain_payload[5] = scan5;
    plain_payload[6] = scan6;

    temp_counter = (aes_counter & 0xf);
    temp_counter = aes_base + (aes_counter & 0xf);
    logitacker_unifying_crypto_encrypt_keyboard_frame(rf_frame, plain_payload, temp_counter);

    if (scan1 == 0 && scan2 == 0 && scan3 == 0 && scan4 == 0 && scan5 == 0 && scan6 == 0)
        ret = radiowrite(rf_frame, 22, "encrypted key up", 1);
    else
        ret = radiowrite(rf_frame, 22, "encrypted key down", 1);

    if (ret)
        // aes_counter++;
        aes_base++;
}

void ludevice::update_little_known_secret_counter(uint8_t *counter_bytes)
{
    memcpy(little_known_secret + 7, counter_bytes, 4);
}

void ludevice::logitacker_unifying_crypto_calculate_frame_key(uint8_t *ciphertext, uint8_t *counter_bytes)
{
    if (RadioDebug)
        printf("1. last plain l_k_s:                 %s\r\n", hexs(little_known_secret, 16));
    update_little_known_secret_counter(counter_bytes); // copy counter_bytes into little_known_secret
    if (RadioDebug)
        printf("2. plain l_k_s+counter:              %s\r\n", hexs(little_known_secret, 16));

    if (RadioDebug)
        printf("3. device_key:                       %s\r\n", hexs(Config->device_key, 16));
    memcpy(ciphertext, little_known_secret, 16); // copy little_known_secret into ciphertext
    AES_ECB_encrypt(&ctx, ciphertext);           // encrypt ciphertext

    if (RadioDebug)
        printf("4. frame_key:                        %s\r\n", hexs(ciphertext, 16));
    return;
}

void ludevice::logitacker_unifying_crypto_encrypt_keyboard_frame(uint8_t *rf_frame, uint8_t *plain_payload, uint32_t counter)
{
    rf_frame[1] = LOGITACKER_DEVICE_REPORT_TYPES_ENCRYPTED_KEYBOARD | LOGITACKER_DEVICE_REPORT_TYPES_KEEP_ALIVE | 0x80;

    uint8_t counter_bytes[4] = {0};
    // K800
    // counter_bytes[3] = (uint8_t)((counter & 0xff000000) >> 24);
    // counter_bytes[2] = (uint8_t)((counter & 0x00ff0000) >> 16);
    // counter_bytes[1] = (uint8_t)((counter & 0x0000ff00) >> 8);
    // counter_bytes[0] = (uint8_t)((counter & 0x000000ff) >> 0);

    // K270
    counter_bytes[0] = (uint8_t)((counter & 0xff000000) >> 24);
    counter_bytes[1] = (uint8_t)((counter & 0x00ff0000) >> 16);
    counter_bytes[2] = (uint8_t)((counter & 0x0000ff00) >> 8);
    counter_bytes[3] = (uint8_t)((counter & 0x000000ff) >> 0);
    memcpy(rf_frame + 10, counter_bytes, 4);

    uint8_t frame_key[16] = {0};
    logitacker_unifying_crypto_calculate_frame_key(frame_key, counter_bytes);

    plain_payload[7] = 0xC9;
    memcpy(rf_frame + 2, plain_payload, 8);

    if (RadioDebug)
        printf("5. plain rf_frame:             %s\r\n", hexs(rf_frame, 22));

    for (int i = 0; i < 8; i++)
        rf_frame[2 + i] ^= frame_key[i];

    setChecksum(rf_frame, 22);

    if (RadioDebug)
        printf("6. encrypted rf_frame:         %s\r\n", hexs(rf_frame, 22));
}

bool ludevice::connected()
{
    return Status & luStatus::Connected;
}