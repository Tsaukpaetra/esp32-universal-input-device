#include <Arduino.h>
#include <elapsedMillis.h>
#include "ludevice.h"

elapsedMillis move_timer;
ludevice kespb(2, 27);

float mouseSpeed = 10.0f;
float degreesToRadians = 2.0f * 3.14f / 360.0f;
bool keydown = false;
int reps = 0;
void luStatusUpdated(luStatus currentStatus);
void setup()
{
    int retcode;
    Serial.setDebugOutput(true);
    Serial.begin(115200);
    Serial.println("Starting");

    SPI.begin(14, 12, 13, 15);

    kespb.statusUpdateFunction = luStatusUpdated;
    //kespb.RadioDebug = true;

    Serial.println(kespb.begin());
}

int funtest = 0;
void loop()
{
    kespb.loop();
    if (kespb.connected() && reps < 10)
    {
        if (funtest < 1 && move_timer > 4000)
        {
            funtest = 1;
            // keydown
            Serial.println("Press keys");

                // encrypted payload: a, b, c, d, e, f
                //kespb.typee(4);
                // kespb.typep(4, 5, 6, 7, 8, 9); // plain payload: a, b, c, d, e, f
                // kespb.typem(0x192, 0); // calculator
                // kespb.typem(0x183, 0); // video player

                kespb.typem(0xe2); // toggle mute
                //kespb.typem(0xe9); // volume up
                //kespb.typem(0xea); // volume down
                // kespb.typem(0xb6); // rev
                // kespb.typem(0xcd); // play/pause
                // kespb.typem(0xb5); // fwd

        }
        if (funtest < 2 && move_timer > 4500)
        {
            funtest = 2;
            Serial.println("Release keys");
            kespb.typep();                        // plain release all keys
            kespb.typee();                        // encrypted release all keys
            kespb.typem();                        // multimedia release all keys
        }
        

        if (funtest < 3 && move_timer > 5000)
        {
            funtest = 3;
            int x, y = 0;

            Serial.println("moving mouse ");
            for (x = 0; x < 360; x += 5)
            {
                Serial.print(".");

                kespb.move((uint16_t)(mouseSpeed * cos(((float)x) * degreesToRadians)),
                           (uint16_t)(mouseSpeed * sin(((float)x) * degreesToRadians)));

                delay(10);
                // kespb.typee();
            }
            Serial.println("");
        }


        if (move_timer > 5000 && move_timer < 6000)
        {
            int x, y = 0;

            Serial.println("moving mouse ");
            for (x = 0; x < 360; x += 5)
            {
                Serial.print(".");

                kespb.move((uint16_t)(mouseSpeed * cos(((float)x) * degreesToRadians)),
                           (uint16_t)(mouseSpeed * sin(((float)x) * degreesToRadians)));

                delay(10);
                // kespb.typee();
            }
            Serial.println("");
        }
        if (move_timer > 10000)
        {
            kespb.move(0, 0, 0, 0, false, false); // left click up
            kespb.typep();                        // plain release all keys
            kespb.typee();                        // encrypted release all keys
            kespb.typem();                        // multimedia release all keys
            keydown = false;
            move_timer = 0;
            funtest = 0;
            reps++;
        }
    }
}

void luStatusUpdated(luStatus currentStatus)
{
    Serial.print("LuStatus:");
    Serial.println(currentStatus);
}