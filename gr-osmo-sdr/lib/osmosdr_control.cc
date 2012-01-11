#include "osmosdr_control.h"

osmosdr_control::osmosdr_control(const std::string &device)
{
    /* lookup acm control channel device name for a given alsa device name */
}

osmosdr_control::~osmosdr_control()
{
}

osmosdr_rx_control::osmosdr_rx_control(const std::string &device) :
    osmosdr_control(device)
{
}

osmosdr_rx_control::~osmosdr_rx_control()
{
}

osmosdr_tx_control::osmosdr_tx_control(const std::string &device) :
    osmosdr_control(device)
{
}

osmosdr_tx_control::~osmosdr_tx_control()
{
}
