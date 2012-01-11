#ifndef OSMOSDR_CONTROL_H
#define OSMOSDR_CONTROL_H

#include <string>

/*! session object to cdc-acm based control channel of the device */
class osmosdr_control
{
public:
    osmosdr_control(const std::string &device);
    virtual ~osmosdr_control();

    /* TODO: provide facilities for device discovery (lookup, alsa to ttyS... ) */
};

/*! osmosdr_source class derives this class to be able to control the device */
class osmosdr_rx_control : public osmosdr_control
{
public:
    osmosdr_rx_control(const std::string &device);
    virtual ~osmosdr_rx_control();

    /*! \brief Set frequency with Hz resolution.
     *  \param freq The frequency in Hz
     *
     * This function takes a double parameter in order to allow using engineering
     * notation in GRC.
     *
     * \see set_center_freq()
     */
    int set_center_freq(double freq);

    double get_center_freq();

    /*! \brief Set LNA gain.
     *  \param gain The new gain in dB.
     *
     * Set the LNA gain in the OsmoSDR. Valid range is -5 to 30. Although
     * the LNA gain in the OsmoSDR takes enumerated values corresponding to
     * 2.5 dB steps, you can can call this method with any double value
     * and it will be rounded to the nearest valid value.
     *
     * By default the LNA gain is set to 20 dB and this is a good value for
     * most cases. In noisy areas you may try to reduce the gain.
     */
    int set_lna_gain(double gain);

    /*! \brief Set mixer gain.
     *  \param gain The new gain in dB.
     *
     * Set the mixer gain in the OsmoSDR. Valid values are +4 and +12 dB.
     *
     * By default the mixer gain is set to +12 dB and this is a good value for
     * most cases. In noisy areas you may try to reduce the gain.
     */
    int set_mixer_gain(double gain);

    /*! magically automates gain selection by using all available gain stages */
    int set_gain(double gain);

    /*! \brief Set IF stage gain.
     *  \param gain The new gain in dB.
     *
     * Set the IF stage gain in the OsmoSDR. Valid range is TBD to TBD.
     *
     * By default the IF gain is set to TBD dB and this is a good value for most
     * cases. In noisy areas you may try to reduce the gain.
     */
    int set_if_gain(double gain);

    /*! \brief Set new frequency correction.
     *  \param ppm The new frequency correction in parts per million
     *
     *  TODO: document recommended carrection values (if any)
     */
    int set_freq_correction(int ppm);

    /*! \brief Set DC offset correction.
     *  \param in_phase DC correction for I component (-1.0 to 1.0)
     *  \param quadrature DC correction for Q component (-1.0 to 1.0)
     *
     * Set DC offset correction in the device. Default is 0.0.
     */
    int set_dc_correction(double in_phase, double quadrature);

    /*! \brief Set IQ phase and gain balance.
     *  \param gain The gain correction (-1.0 to 1.0)
     *  \param phase The phase correction (-1.0 to 1.0)
     *
     * Set IQ phase and gain balance in the device. The default values
     * are 0.0 for phase and 1.0 for gain.
     */
    int set_iq_correction(double gain, double phase);

    /*! TODO: add more */

};

/*! osmosdr_sink class derives this class to be able to control the device */
class osmosdr_tx_control : public osmosdr_control
{
public:
    osmosdr_tx_control(const std::string &device);
    virtual ~osmosdr_tx_control();

    /*! \brief Set frequency with Hz resolution.
     *  \param freq The frequency in Hz
     *
     * This function takes a double parameter in order to allow using engineering
     * notation in GRC.
     *
     * \see set_center_freq()
     */
    int set_center_freq(double freq);

    double get_center_freq();

    /*! \brief Set new frequency correction.
     *  \param ppm The new frequency correction in parts per million
     *
     *  TODO: document recommended carrection values (if any)
     */
    int set_freq_correction(int ppm);

    /*! \brief Set DC offset correction.
     *  \param in_phase DC correction for I component (-1.0 to 1.0)
     *  \param quadrature DC correction for Q component (-1.0 to 1.0)
     *
     * Set DC offset correction in the device. Default is 0.0.
     */
    int set_dc_correction(double in_phase, double quadrature);

    /*! \brief Set IQ phase and gain balance.
     *  \param gain The gain correction (-1.0 to 1.0)
     *  \param phase The phase correction (-1.0 to 1.0)
     *
     * Set IQ phase and gain balance in the device. The default values
     * are 0.0 for phase and 1.0 for gain.
     */
    int set_iq_correction(double gain, double phase);

    /*! TODO: add more */
};

#endif // OSMOSDR_CONTROL_H
