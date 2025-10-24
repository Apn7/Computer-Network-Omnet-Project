#include <omnetpp.h>

using namespace omnetpp;

int messagePassed = 1;

class Simple_Module_1: public cSimpleModule
{
protected:
    void initialize() override;
    void handleMessage(cMessage *msg) override;
};


Define_Module(Simple_Module_1);

void Simple_Module_1::initialize()
{
    if (strcmp(getName(), "computer1")) {
        cMessage *msg = new cMessage("Hey!");
        send(msg, "out1");
    }
}


void Simple_Module_1::handleMessage(cMessage *msg)
{
    cMessage *msg2 = new cMessage("Hey!", messagePassed++);
    send(msg2, "out1");
}
