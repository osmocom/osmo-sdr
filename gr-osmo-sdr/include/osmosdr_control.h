#ifndef OSMOSDR_CONTROL_H
#define OSMOSDR_CONTROL_H

#include <string>
#include <complex>
#include <osmosdr_ranges.h>

/*!
 * session object to cdc-acm based control channel of the device
 */
class osmosdr_control
{
public:
    osmosdr_control(const std::string &addr);
    virtual ~osmosdr_control();

    /*!
     * Discovers all devices connected to the host computer.
     * \return a vector of device addresses
     */
    static std::vector< std::string > find_devices();

protected:
    std::string audio_dev_name();
    std::string control_dev_name();
};

/*!
 * osmosdr_source class derives from this class to be able to control the device
 */
class osmosdr_rx_control : public osmosdr_control
{
public:
    osmosdr_rx_control(const std::string &addr);
    virtual ~osmosdr_rx_control();
#if 0
    /*!
     * Set the sample rate for the OsmoSDR device.
     * This also will select the appropriate IF bandpass.
     * \param rate a new rate in Sps
     */
    virtual void set_samp_rate(double rate) = 0;

    /*!
     * Get the sample rate for the OsmoSDR device.
     * This is the actual sample rate and may differ from the rate set.
     * \return the actual rate in Sps
     */
    virtual double get_samp_rate(void) = 0;

    /*!
     * Get the possible sample rates for the OsmoSDR device.
     * \return a range of rates in Sps
     */
    virtual osmosdr::meta_range_t get_samp_rates(void) = 0;

    /*!
     * Tune the OsmoSDR device to the desired center frequency.
     * This also will select the appropriate RF bandpass.
     * \param freq the desired frequency in Hz
     * \return a tune result with the actual frequencies
     */
    virtual bool set_center_freq(double freq) = 0;

    /*!
     * Get the center frequency.
     * \return the frequency in Hz
     */
    virtual double get_center_freq() = 0;

    /*!
     * Get the tunable frequency range.
     * \return the frequency range in Hz
     */
    virtual osmosdr::freq_range_t get_freq_range() = 0;

    /*!
     * Set the gain for the OsmoSDR.
     * \param gain the gain in dB
     */
    virtual void set_gain(double gain) = 0;

    /*!
     * Set the named gain on the OsmoSDR.
     * \param gain the gain in dB
     * \param name the name of the gain stage
     */
    virtual void set_gain(double gain, std::string &name) = 0;

    /*!
     * Get the actual OsmoSDR gain setting.
     * \return the actual gain in dB
     */
    virtual double get_gain() = 0;

    /*!
     * Get the actual OsmoSDR gain setting of named stage.
     * \param name the name of the gain stage
     * \return the actual gain in dB
     */
    virtual double get_gain(const std::string &name) = 0;

    /*!
     * Get the actual OsmoSDR gain setting of named stage.
     * \return the actual gain in dB
     */
    virtual std::vector<std::string> get_gain_names() = 0;

    /*!
     * Get the settable gain range.
     * \return the gain range in dB
     */
    virtual osmosdr::gain_range_t get_gain_range() = 0;

    /*!
     * Get the settable gain range.
     * \param name the name of the gain stage
     * \return the gain range in dB
     */
    virtual osmosdr::gain_range_t get_gain_range(const std::string &name) = 0;

    /*!
     * Enable/disable the automatic DC offset correction.
     * The automatic correction subtracts out the long-run average.
     *
     * When disabled, the averaging option operation is halted.
     * Once halted, the average value will be held constant
     * until the user re-enables the automatic correction
     * or overrides the value by manually setting the offset.
     *
     * \param enb true to enable automatic DC offset correction
     */
    virtual void set_dc_offset(const bool enb) = 0;

    /*!
     * Set a constant DC offset value.
     * The value is complex to control both I and Q.
     * Only set this when automatic correction is disabled.
     * \param offset the dc offset (1.0 is full-scale)
     */
    virtual void set_dc_offset(const std::complex<double> &offset) = 0;

    /*!
     * Set the RX frontend IQ imbalance correction.
     * Use this to adjust the magnitude and phase of I and Q.
     *
     * \param correction the complex correction value
     */
    virtual void set_iq_balance(const std::complex<double> &correction) = 0;

    /*!
     * Get the master clock rate.
     * \return the clock rate in Hz
     */
    virtual double get_clock_rate() = 0;

    /*!
     * Set the master clock rate.
     * \param rate the new rate in Hz
     */
    virtual void set_clock_rate(double rate) = 0;

    /*!
     * Get automatic gain control status
     * \return the clock rate in Hz
     */
    virtual bool get_agc() = 0;

    /*!
     * Enable/disable the automatic gain control.
     * \param enb true to enable automatic gain control
     * \param attack attack time of the AGC circuit
     * \param decay decay time of the AGC circuit
     */
    virtual void set_agc(bool enb, double attack, double decay) = 0;
#endif
    /*! TODO: add more */
};

/*!
 * osmosdr_sink class derives from this class to be able to control the device
 */
class osmosdr_tx_control : public osmosdr_control
{
public:
    osmosdr_tx_control(const std::string &addr);
    virtual ~osmosdr_tx_control();

    /*! TODO: add more */
};

#endif // OSMOSDR_CONTROL_H
