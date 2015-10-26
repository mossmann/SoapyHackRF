/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2015
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "SoapyHackRF.hpp"


SoapyHackRF::SoapyHackRF( const SoapySDR::Kwargs &args )
{
	_running	= false;

	_auto_bandwidth=true;
	
	_dev		= NULL;

	_rx_vga_gain = 20;

	_rx_lna_gain = 16;

	_tx_vga_gain = 0;

	_amp = 0;

	_frequency = 1000000;

	_samplerate = 2000000;

	_bandwidth=0;

	_id = -1;

	_buf_num	= BUF_NUM;
	_buf_len	= BUF_LEN;

	hackrf_device_list_t	* list = hackrf_device_list();

	if ( args.count( "hackrf" ) != 0 )
	{
		int _id = std::stoi( args.at( "hackrf" ) );
		
		if ( _id < 0 && _id > list->devicecount )
		{
			throw std::runtime_error( "hackrf out of range [0 .. " + std::to_string( list->devicecount ) + "]." );
		}
		int ret = hackrf_device_list_open(list, _id, &_dev );
		if ( ret != HACKRF_SUCCESS )
		{
			hackrf_device_list_free(list);
			SoapySDR_logf( SOAPY_SDR_INFO, "Could not Open HackRF Device by Index:%d", _id );
			throw std::runtime_error("hackrf open failed");
		}
	} else if ( _id == -1 )
	{
		int ret = hackrf_open( &_dev );
		if ( ret != HACKRF_SUCCESS )
		{
			SoapySDR_logf( SOAPY_SDR_INFO, "Could not Open HackRF Device" );
			throw std::runtime_error("hackrf open failed");
		}
	}
	
	hackrf_device_list_free(list);

}


SoapyHackRF::~SoapyHackRF( void )
{
	if ( _dev )
	{
		hackrf_close( _dev );
	}

	/* cleanup device handles */
}


/*******************************************************************
 * Identification API
 ******************************************************************/

std::string SoapyHackRF::getDriverKey( void ) const
{

	return("HackRF");
}


std::string SoapyHackRF::getHardwareKey( void ) const
{
	uint8_t board_id=BOARD_ID_INVALID;

	hackrf_board_id_read(_dev,&board_id);

	return(hackrf_board_id_name((hackrf_board_id)board_id));
}


SoapySDR::Kwargs SoapyHackRF::getHardwareInfo( void ) const
{
	SoapySDR::Kwargs info;

	char version_str[100];

	hackrf_version_string_read(_dev, &version_str[0], 100);

	info["version"] = version_str;

	read_partid_serialno_t read_partid_serialno;

	hackrf_board_partid_serialno_read(_dev, &read_partid_serialno);

	char part_id_str[100];

	sprintf(part_id_str, "%08x%08x", read_partid_serialno.part_id[0], read_partid_serialno.part_id[1]);

	info["part id"] = part_id_str;

	char serial_str[100];
	sprintf(serial_str, "%08x%08x%08x%08x", read_partid_serialno.serial_no[0], read_partid_serialno.serial_no[1], read_partid_serialno.serial_no[2], read_partid_serialno.serial_no[3]);
	info["serial"] = serial_str;

	uint16_t clock;

	hackrf_si5351c_read(_dev,0,&clock);

	info["clock source"]=(clock==0x51)?"internal":"external";

	return(info);

}


/*******************************************************************
 * Channels API
 ******************************************************************/

size_t SoapyHackRF::getNumChannels( const int dir ) const
{
	return(1);
}


bool SoapyHackRF::getFullDuplex( const int direction, const size_t channel ) const
{
	return(false);
}

/*******************************************************************
 * Settings API
 ******************************************************************/

SoapySDR::ArgInfoList SoapyHackRF::getSettingInfo(void) const
{
	SoapySDR::ArgInfoList setArgs;

	SoapySDR::ArgInfo biastxArg;
	biastxArg.key="bias_tx";
	biastxArg.value="false";
	biastxArg.name="Antenna Bias";
	biastxArg.description="Antenna port power control.";
	biastxArg.type=SoapySDR::ArgInfo::BOOL;
	setArgs.push_back(biastxArg);

	return setArgs;
}

void SoapyHackRF::writeSetting(const std::string &key, const std::string &value)
{
	if(key=="bias_tx"){
		_bias=(value=="true") ? true : false;
		int ret=hackrf_set_antenna_enable(_dev,_bias);
		if(ret!=HACKRF_SUCCESS){

			SoapySDR_logf(SOAPY_SDR_INFO,"Failed to apply antenna bias voltage");

		}
	}

}

std::string SoapyHackRF::readSetting(const std::string &key) const
{
	if (key == "bias_tx") {
		return _bias?"true":"false";
	}
	return "";
}
/*******************************************************************
 * Antenna API
 ******************************************************************/

std::vector<std::string> SoapyHackRF::listAntennas( const int direction, const size_t channel ) const
{
	std::vector<std::string> options;
	options.push_back( "TX/RX" );
	return(options);
}


void SoapyHackRF::setAntenna( const int direction, const size_t channel, const std::string &name )
{
	/* TODO delete this function or throw if name != RX... */
}


std::string SoapyHackRF::getAntenna( const int direction, const size_t channel ) const
{
	return("TX/RX");
}


/*******************************************************************
 * Frontend corrections API
 ******************************************************************/


bool SoapyHackRF::hasDCOffsetMode( const int direction, const size_t channel ) const
{
	return(false);
}


/*******************************************************************
 * Gain API
 ******************************************************************/

std::vector<std::string> SoapyHackRF::listGains( const int direction, const size_t channel ) const
{
	std::vector<std::string> options;
	if ( direction == SOAPY_SDR_RX )
	{
		options.push_back( "LNA" );
	}
	options.push_back( "VGA" );
	options.push_back( "AMP" );

	return(options);
	/*
	 * list available gain elements,
	 * the functions below have a "name" parameter
	 */
}


void SoapyHackRF::setGainMode( const int direction, const size_t channel, const bool automatic )
{
	/* enable AGC if the hardware supports it, or remove this function */
}


bool SoapyHackRF::getGainMode( const int direction, const size_t channel ) const
{
	return(false);
	/* ditto for the AGC */
}


void SoapyHackRF::setGain( const int direction, const size_t channel, const double value )
{
	int32_t ret, gain;
	gain = value;

	if ( direction == SOAPY_SDR_RX )
	{
		if ( gain <= 0 )
		{
			_rx_lna_gain	= 0;
			_rx_vga_gain	= 0;
			_amp		= 0;
		}else if ( gain <= (HACKRF_RX_LNA_MAX_DB / 2) + (HACKRF_RX_VGA_MAX_DB / 2) )
		{
			_rx_vga_gain	= (gain / 3) & ~0x1;
			_rx_lna_gain	= gain - _rx_vga_gain;
			_amp		= 0;
		}else if ( gain <= ( (HACKRF_RX_LNA_MAX_DB / 2) + (HACKRF_RX_VGA_MAX_DB / 2) + HACKRF_AMP_MAX_DB) )
		{
			_amp		= HACKRF_AMP_MAX_DB;
			_rx_vga_gain	= ( (gain - _amp) / 3) & ~0x1;
			_rx_lna_gain	= gain - _amp - _rx_vga_gain;
		}else if ( gain <= HACKRF_RX_LNA_MAX_DB + HACKRF_RX_VGA_MAX_DB + HACKRF_AMP_MAX_DB )
		{
			_amp		= HACKRF_AMP_MAX_DB;
			_rx_vga_gain	= (gain - _amp) * double(HACKRF_RX_LNA_MAX_DB) / double(HACKRF_RX_VGA_MAX_DB);
			_rx_lna_gain	= gain - _amp - _rx_vga_gain;
		}

		ret	= hackrf_set_lna_gain( _dev, _rx_lna_gain );
		ret	|= hackrf_set_vga_gain( _dev, _rx_vga_gain );
		ret	|= hackrf_set_amp_enable( _dev, (_amp > 0) ? 1 : 0 );
	}else if ( direction == SOAPY_SDR_TX )
	{
		if ( gain <= 0 )
		{
			_amp		= 0;
			_tx_vga_gain	= 0;
		}else if ( gain <= (HACKRF_TX_VGA_MAX_DB / 2) )
		{
			_amp		= 0;
			_tx_vga_gain	= gain;
		}else if ( gain <= HACKRF_TX_VGA_MAX_DB + HACKRF_AMP_MAX_DB )
		{
			_amp		= HACKRF_AMP_MAX_DB;
			_tx_vga_gain	= gain - HACKRF_AMP_MAX_DB;
		}

		ret	= hackrf_set_txvga_gain( _dev, _tx_vga_gain );
		ret	|= hackrf_set_amp_enable( _dev, (_amp > 0) ? 1 : 0 );
	}
	if ( ret != HACKRF_SUCCESS )
	{
		SoapySDR::logf( SOAPY_SDR_ERROR, "setGain(%f) returned %s", value, hackrf_error_name( (hackrf_error) ret ) );
	}
}


void SoapyHackRF::setGain( const int direction, const size_t channel, const std::string &name, const double value )
{
	if ( name == "AMP" )
	{
		_amp = value;
		_amp = (_amp > 0)?HACKRF_AMP_MAX_DB : 0; //clip to possible values
		if ( _dev != NULL )
		{
			int ret = hackrf_set_amp_enable( _dev, (_amp > 0)?1 : 0 );
			if ( ret != HACKRF_SUCCESS )
			{
				SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_amp_enable(%f) returned %s", _amp, hackrf_error_name( (hackrf_error) ret ) );
			}
		}
	}else if ( direction == SOAPY_SDR_RX and name == "LNA" )
	{
		_rx_lna_gain = value;
		if ( _dev != NULL )
		{
			int ret = hackrf_set_lna_gain( _dev, _rx_lna_gain );
			if ( ret != HACKRF_SUCCESS )
			{
				SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_lna_gain(%f) returned %s", _rx_lna_gain, hackrf_error_name( (hackrf_error) ret ) );
			}
		}
	}else if ( direction == SOAPY_SDR_RX and name == "VGA" )
	{
		_rx_vga_gain = value;
		if ( _dev != NULL )
		{
			int ret = hackrf_set_vga_gain( _dev, _rx_vga_gain );
			if ( ret != HACKRF_SUCCESS )
			{
				SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_vga_gain(%f) returned %s", _rx_vga_gain, hackrf_error_name( (hackrf_error) ret ) );
			}
		}
	}else if ( direction == SOAPY_SDR_TX and name == "VGA" )
	{
		_tx_vga_gain = value;
		if ( _dev != NULL )
		{
			int ret = hackrf_set_txvga_gain( _dev, _tx_vga_gain );
			if ( ret != HACKRF_SUCCESS )
			{
				SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_txvga_gain(%f) returned %s", _tx_vga_gain, hackrf_error_name( (hackrf_error) ret ) );
			}
		}
	}


	/* set individual gain element by name */
}


double SoapyHackRF::getGain( const int direction, const size_t channel, const std::string &name ) const
{
	double gain = 0.0;
	if ( name == "AMP" )
	{
		gain = _amp;
	}else if ( direction == SOAPY_SDR_RX and name == "LNA" )
	{
		gain = _rx_lna_gain;
	}else if ( direction == SOAPY_SDR_RX and name == "VGA" )
	{
		gain = _rx_vga_gain;
	}else if ( direction == SOAPY_SDR_TX and name == "VGA" )
	{
		gain = _tx_vga_gain;
	}

	return(gain);
}


SoapySDR::Range SoapyHackRF::getGainRange( const int direction, const size_t channel, const std::string &name ) const
{
	if ( name == "AMP" )
		return(SoapySDR::Range( 0, HACKRF_AMP_MAX_DB ) );
	if ( direction == SOAPY_SDR_RX and name == "LNA" )
		return(SoapySDR::Range( 0, HACKRF_RX_LNA_MAX_DB ) );
	if ( direction == SOAPY_SDR_RX and name == "VGA" )
		return(SoapySDR::Range( 0, HACKRF_RX_VGA_MAX_DB ) );
	if ( direction == SOAPY_SDR_TX and name == "VGA" )
		return(SoapySDR::Range( 0, HACKRF_TX_VGA_MAX_DB ) );
	return(SoapySDR::Range( 0, 0 ) );
}


/*******************************************************************
 * Frequency API
 ******************************************************************/

void SoapyHackRF::setFrequency( const int direction, const size_t channel, const std::string &name, const double frequency, const SoapySDR::Kwargs &args )
{
	if ( name == "BB" )
		return;
	if ( name != "RF" )
		throw std::runtime_error( "setFrequency(" + name + ") unknown name" );

	_frequency = frequency;

	if ( _dev != NULL )
	{
		int ret = hackrf_set_freq( _dev, _frequency );

		if ( ret != HACKRF_SUCCESS )
		{
			SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_freq(%f) returned %s", _frequency, hackrf_error_name( (hackrf_error) ret ) );
		}
	}
}


double SoapyHackRF::getFrequency( const int direction, const size_t channel, const std::string &name ) const
{
	if ( name == "BB" )
		return(0.0);
	if ( name != "RF" )
		throw std::runtime_error( "getFrequency(" + name + ") unknown name" );
	return(_frequency);
}

SoapySDR::ArgInfoList SoapyHackRF::getFrequencyArgsInfo(const int direction, const size_t channel) const
{
	SoapySDR::ArgInfoList freqArgs;
	// TODO: frequency arguments
	return freqArgs;
}

std::vector<std::string> SoapyHackRF::listFrequencies( const int direction, const size_t channel ) const
{
	std::vector<std::string> names;
	names.push_back( "RF" );
	return(names);
}


SoapySDR::RangeList SoapyHackRF::getFrequencyRange( const int direction, const size_t channel, const std::string &name ) const
{
	if ( name == "BB" )
		return(SoapySDR::RangeList( 1, SoapySDR::Range( 0.0, 0.0 ) ) );
	if ( name != "RF" )
		throw std::runtime_error( "getFrequencyRange(" + name + ") unknown name" );
	return(SoapySDR::RangeList( 1, SoapySDR::Range( 0, 7250000000ull ) ) );
}


/*******************************************************************
 * Sample Rate API
 ******************************************************************/

void SoapyHackRF::setSampleRate( const int direction, const size_t channel, const double rate )
{
	_samplerate = rate;

	if ( _dev != NULL )
	{
		int ret = hackrf_set_sample_rate( _dev, _samplerate );

		if(_auto_bandwidth){

			_bandwidth=hackrf_compute_baseband_filter_bw_round_down_lt(_samplerate);

			ret|=hackrf_set_baseband_filter_bandwidth(_dev,_bandwidth);
		}

		if ( ret != HACKRF_SUCCESS )
		{
			SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_sample_rate(%f) returned %s", _samplerate, hackrf_error_name( (hackrf_error) ret ) );
			throw std::runtime_error( "setSampleRate()" );
		}
	}
}


double SoapyHackRF::getSampleRate( const int direction, const size_t channel ) const
{
	return(_samplerate);
}


std::vector<double> SoapyHackRF::listSampleRates( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	for ( double r = 1e6; r <= 20e6; r += 1e6 )
	{
		options.push_back( r );
	}
	return(options);
}


void SoapyHackRF::setBandwidth( const int direction, const size_t channel, const double bw )
{
	_bandwidth = hackrf_compute_baseband_filter_bw(bw);

	if(_bandwidth!=0){
		_auto_bandwidth=false;

		if ( _dev != NULL )
		{
			int ret = hackrf_set_baseband_filter_bandwidth( _dev, _bandwidth );
			if ( ret != HACKRF_SUCCESS )
			{
				SoapySDR::logf( SOAPY_SDR_ERROR, "hackrf_set_baseband_filter_bandwidth(%f) returned %s", _bandwidth, hackrf_error_name( (hackrf_error) ret ) );
				throw std::runtime_error( "setBandwidth()" );
			}
		}

	}else{
		_auto_bandwidth=true;
	}

}


double SoapyHackRF::getBandwidth( const int direction, const size_t channel ) const
{
	return(_bandwidth);
}


std::vector<double> SoapyHackRF::listBandwidths( const int direction, const size_t channel ) const
{
	std::vector<double> options;
	options.push_back( 1750000 );
	options.push_back( 2500000 );
	options.push_back( 3500000 );
	options.push_back( 5000000 );
	options.push_back( 5500000 );
	options.push_back( 6000000 );
	options.push_back( 7000000 );
	options.push_back( 8000000 );
	options.push_back( 9000000 );
	options.push_back( 10000000 );
	options.push_back( 12000000 );
	options.push_back( 14000000 );
	options.push_back( 15000000 );
	options.push_back( 20000000 );
	options.push_back( 24000000 );
	options.push_back( 28000000 );
	return(options);
}


