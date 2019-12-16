#include "baro.h"

namespace sensor_simulator
{
namespace sensor
{

Baro::Baro(std::shared_ptr<Ekf> ekf):Sensor(ekf)
{
}

Baro::~Baro()
{
}

void Baro::send(uint32_t time)
{
	_ekf->setBaroData(time,_baro_data);
	_time_last_data_sent = time;
}

void Baro::setData(float baro)
{
	_baro_data = baro;
}

} // namespace sensor
} // namespace sensor_simulator
