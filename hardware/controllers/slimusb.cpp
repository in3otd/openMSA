/**
 ******************************************************************************
 *
 * @file       main.cpp
 * @author     Jose Barros (AKA PT_Dreamer) josemanuelbarros@gmail.com 2018
 * @brief      slimusb.cpp file
 * @see        The GNU Public License (GPL) Version 3
 * @defgroup   slimusb
 * @{
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <http://www.gnu.org/licenses/>
 */
#include "slimusb.h"
#include <QDebug>
#include "../deviceparser.h"
#include "../lmx2326.h"
#include "../ad9850.h"
#include "../msa.h"
slimusb::slimusb(QObject *parent): interface(parent), usb(parent), autoConnect(true), singleStep(false), writeReadDelay_us(100), isPaused(false),
	pll1data(NULL),pll1le(NULL),dds1data(NULL),dds1fqu(NULL),pll2data(NULL),pll2le(NULL),dds3data(NULL),dds3fqu(NULL)
	,pll3data(NULL),pll3le(NULL),pll1(NULL),pll2(NULL),pll3(NULL),dds1(NULL),dds3(NULL),adcmag(NULL),adcph(NULL),pll1array(NULL)
	,pll3array(NULL),dds1array(NULL),pll2array(NULL),dds3array(NULL)
{
	lastCommandedStep = 0;
	usbB2union.command.adcMAG = 0;
	usbB2union.command.adcPhase = 0;
	currentLatchValue.resize(5);
	latchToUSBNumber.insert(1,1);
	latchToUSBNumber.insert(2,3);
	latchToUSBNumber.insert(3,0);
	latchToUSBNumber.insert(4,2);
	latchToUSBNumber.insert(7,7);
	dds1 = dds3 = NULL;
	pll1 = pll2 = pll3 = NULL;
	adcmag = adcph = NULL;
	connect(&usb, SIGNAL(connected()), this, SIGNAL(connected()));
	connect(&usb, SIGNAL(disconnected()), this, SIGNAL(disconnected()));
	usbB2union.data[0] = 0xB2;
	usbB2union.data[8] = 0xA4;
	usbB2union.data[9] = 0x14;
	usbB2union.data[10] = 0x00;
	usbB2union.data[11] = 0x00;
	qDebug() << "XXXXXXXXXXXXXXXXXXXXXXX" << usbB2union.command.adcMAG;
}

bool slimusb::init(int debugLevel)
{
	if(!usb.init((debugLevel)))
		return false;
	if(autoConnect)
		usb.enableCallBack(true);
	return true;
}

slimusb::~slimusb()
{
	qDeleteAll(usbData.values());
	this->requestInterruption();
	this->wait(1000);
	foreach (hardwareDevice *dev, msa::getInstance().currentHardwareDevices) {
		foreach (hardwareDevice::devicePin *pin, dev->devicePins.values()) {
			if(pin->hwconfig)
				delete (parallelEqui*)pin->hwconfig;
		}
	}
//	if(pll1data)
//		delete pll1data;
//	if(pll1le)
//		delete pll1le;
//	if(dds1data)
//		delete dds1data;
//	if(dds1fqu)
//		delete dds1fqu;
//	if(pll2data)
//		delete pll2data;
//	if(pll2le)
//		delete pll2le;
//	if(dds3data)
//		delete dds3data;
//	if(dds3fqu)
//		delete dds3fqu;
//	if(pll3data)
//		delete pll3data;
//	if(pll3le)
//		delete pll3le;
//	if(pll1)
//		delete pll1;
//	if(pll2)
//		delete pll2;
//	if(pll3)
//		delete pll3;
//	if(dds1)
//		delete dds1;
//	if(dds3)
//		delete dds3;
//	if(adcmag)
//		delete adcmag;
//	if(adcph)
//		delete adcph;
//	if(pll1array)
//		delete pll1array;
//	if(pll3array)
//		delete pll3array;
//	if(dds1array)
//		delete dds1array;
//	if(pll2array)
//		delete pll2array;
//	if(dds3array)
//		delete dds3array;

}

void slimusb::commandStep(int step)
{
	sendUSB(*usbData.value(step), 7, false);
	QThread::usleep(writeReadDelay_us);
	sendUSB(adcSend, 0, false, true);
	lastCommandedStep = step;
}

bool slimusb::getIsConnected() const
{
	return (usbdevice::deviceHandler != NULL);
}

void slimusb::commandNextStep()
{
	commandStep(currentStep);
	QThread::usleep(writeReadDelay_us);
	if(!isInverted)
		++currentStep;
	if(currentStep > (qint32)(numberOfSteps - 1))
		currentStep = 0;
	if(isInverted)
		--currentStep;
	if(currentStep < 0)
		currentStep = numberOfSteps -1;
}

void slimusb::commandPreviousStep()
{
	if(!isInverted)
		--currentStep;
	if(isInverted)
		++currentStep;
	if(currentStep > (qint32)(numberOfSteps - 1))
		currentStep = 0;
	if(currentStep < 0)
		currentStep = numberOfSteps -1;
	commandStep(currentStep);
	QThread::usleep(writeReadDelay_us);
}

void slimusb::initScan(bool inverted, double start, double end, double step, int band)
{
	Q_UNUSED(band);
	QList<hardwareDevice::devicePin*> dataPins;
	int maxSize = 0;
	interface::initScan(inverted, start, end, step);
	pll1->processNewScan();
	dds1->processNewScan();
	pll3->processNewScan();
	dds3->processNewScan();
	qDeleteAll(usbData.values());
	usbData.clear();
	foreach (hardwareDevice *dev, msa::getInstance().currentHardwareDevices.values()) {
		foreach (hardwareDevice::devicePin *pin, dev->getDevicePins().values()) {
			if(pin->IOtype == hardwareDevice::MAIN_DATA) {
				dataPins.append(pin);
				if(pin->data.contains(0) && pin->data.value(0).dataArray->size() > maxSize)
					maxSize = pin->data.value(0).dataArray->size();
			}
		}
	}
	int delta;
	for(int step = 0; step < hardwareDevice::currentScan.steps.size(); ++step) {
		QByteArray *arr = new QByteArray;
		usbData.insert(step, arr);
		for(int b = 0; b < maxSize; ++b) {
			uint8_t byte = currentLatchValue.at(1);
			foreach (hardwareDevice::devicePin *pin, dataPins) {
				if(!pin->hwconfig)
					continue;
				if(pin->data.contains(step)) {
					delta = maxSize - pin->data.value(step).dataArray->size() + pin->data.value(step).dataMask->count(0);

					if(b >= delta) {
						byte = (byte & ((parallelEqui*)(pin->hwconfig))->mask) | pin->data.value(step).dataArray->at(b - delta) << ((parallelEqui*)(pin->hwconfig))->pin;

					}
				}
			}
			arr->append(byte);
		}
	}
	adcSend.clear();
	if(hardwareDevice::currentScan.configuration.scanType != hardwareDevice::VNA) {
		adcSend.append(0xB2);//TODO
	}
	else
		adcSend.append(0xB2);
	if(adcmag && (adcmag->getHardwareType() == hardwareDevice::AD7685)) {
		expectedAdcSize = 16;
		adcSend.append((char)(0x00));
		adcSend.append(0x01); adcSend.append(0x10);
	}
	else {
		adcSend.append(0x01);adcSend.append(0x03);adcSend.append(0x0C);
	}
	adcSend.append(hardwareDevice::currentScan.configuration.adcAveraging);
}

void slimusb::hardwareInit()
{
	interface::hardwareInit();
	QHash<hardwareDevice::MSAdevice, hardwareDevice *> loadedDevices = msa::getInstance().currentHardwareDevices;
	foreach(hardwareDevice* dev, loadedDevices.values()) {
		if((loadedDevices.key(dev) == hardwareDevice::PLL1) || (loadedDevices.key(dev) == hardwareDevice::PLL2) || (loadedDevices.key(dev) == hardwareDevice::PLL3)) {
			genericPLL *pll = qobject_cast<genericPLL*>(dev);
			if(loadedDevices.key(dev) == hardwareDevice::PLL1)
				pll1 = pll;
			if(loadedDevices.key(dev) == hardwareDevice::PLL2)
				pll2 = pll;
			if(loadedDevices.key(dev) == hardwareDevice::PLL3)
				pll3 = pll;
			QHash<int, hardwareDevice::devicePin*> pllpins = pll->getDevicePins();
			hardwareDevice::devicePin *pin;
			parallelEqui *p;
			foreach (int type, pllpins.keys()) {
				pin = pllpins.value(type);
				p = new parallelEqui;
				switch (type) {
				case lmx2326::PIN_DATA:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::PLL1:
						p->latch = 1;
						p->pin = 1;
						pll1data = p;
						break;
					case hardwareDevice::PLL2:
						p->latch = 1;
						p->pin = 4;
						pll2data = p;
						break;
					case hardwareDevice::PLL3:
						p->latch = 1;
						p->pin = 3;
						pll3data = p;
						break;
					default:
						break;
					}
					break;
				case lmx2326::PIN_CLK:
					p->latch = 1;
					p->pin = 0;
					break;
				case lmx2326::PIN_LE:
					p->latch = 2;
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::PLL1:
						p->pin = 0;
						pll1le = p;
						break;
					case hardwareDevice::PLL2:
						p->pin = 4;
						pll2le = p;
						break;
					case hardwareDevice::PLL3:
						p->pin = 2;
						pll3le = p;
						break;
					default:
						break;
					}
					break;
				default:
					pin = pllpins.value(type);
					delete p;
					p = NULL;
					break;
				}
				if(p)
					p->mask = ~(1 << p->pin);
				pin->hwconfig = p;
			}

		}
		else if((loadedDevices.key(dev) == hardwareDevice::DDS1) || (loadedDevices.key(dev) == hardwareDevice::DDS3)) {
			genericDDS *dds = qobject_cast<genericDDS*>(dev);
			if(loadedDevices.key(dev) == hardwareDevice::DDS1)
				dds1 = dds;
			else if(loadedDevices.key(dev) == hardwareDevice::DDS3)
				dds3 = dds;
			QHash<int, hardwareDevice::devicePin*> ddspins = dds->getDevicePins();
			hardwareDevice::devicePin *pin;
			parallelEqui *p;
			foreach (int type, ddspins.keys()) {
				pin = ddspins.value(type);
				p = new parallelEqui;
				switch (type) {
				case ad9850::PIN_DATA:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::DDS1:
						p->latch = 1;
						p->pin = 2;
						dds1data = p;
						break;
					case hardwareDevice::DDS3:
						p->latch = 1;
						p->pin = 4;
						dds3data = p;
						break;
					default:
						break;
					}
					break;
				case ad9850::PIN_WCLK:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::DDS1:
					case hardwareDevice::DDS3:
						p->latch = 1;
						p->pin = 0;
						break;
					default:
						break;
					}
					break;
				case ad9850::PIN_FQUD:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::DDS1:
						p->latch = 2;
						p->pin = 1;
						dds1fqu = p;
						break;
					case ad9850::PIN_FQUD:
					case hardwareDevice::DDS3:
						p->latch = 2;
						p->pin = 3;
						dds3fqu = p;
						break;
					default:
						break;
					}
					break;
				default:
					pin = ddspins.value(type);
					delete p;
					p = NULL;
					break;

				}
				if(p)
					p->mask = ~(1 << p->pin);
				pin->hwconfig = p;
			}
		}
		else if((loadedDevices.key(dev) == hardwareDevice::ADC_MAG) || (loadedDevices.key(dev) == hardwareDevice::ADC_PH)) {
			genericADC *adc = qobject_cast<genericADC*>(dev);
			if(loadedDevices.key(dev) == hardwareDevice::ADC_MAG)
				adcmag = adc;
			else if(loadedDevices.key(dev) == hardwareDevice::ADC_PH)
				adcph = adc;
			QHash<int, hardwareDevice::devicePin*> adcpins = adc->getDevicePins();
			hardwareDevice::devicePin *pin;
			parallelEqui *p;
			foreach (int type, adcpins.keys()) {
				pin = adcpins.value(type);
				p = new parallelEqui;
				switch (type) {
				case genericADC::PIN_DATA:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::ADC_MAG:
						p->latch = 0;
						p->pin = 4;
						break;
					case hardwareDevice::ADC_PH:
						p->latch = 0;
						p->pin = 5;
						break;
					default:
						break;
					}
					break;
				case genericADC::PIN_CLK:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::ADC_MAG:
					case hardwareDevice::ADC_PH:
						p->latch = 3;
						p->pin = 6;
						break;
					default:
						break;
					}
					break;
				case genericADC::PIN_CONVERT:
					switch (loadedDevices.key(dev)) {
					case hardwareDevice::ADC_MAG:
					case hardwareDevice::ADC_PH:
						p->latch = 3;
						p->pin = 7;
						break;
					default:
						break;
					}
					break;
				default:
					pin = adcpins.value(type);
					delete p;
					p = NULL;
					break;
				}
				if(p)
					p->mask = ~(1 << p->pin);
				pin->hwconfig = p;
			}
		}
	}
	if(pll1) {
		switch (pll1->getHardwareType()) {
		case hardwareDevice::LMX2326:
			pll1array = deviceParser::getDeviceList().value(hardwareDevice::PLL1)->devicePins.value(lmx2326::PIN_DATA)->data.value(HW_INIT_STEP).dataArray;
			break;
		default:
			break;
		}
	}
	if(dds1) {
		switch (dds1->getHardwareType()) {
		case hardwareDevice::AD9850:
			dds1array = deviceParser::getDeviceList().value(hardwareDevice::DDS1)->devicePins.value(ad9850::PIN_DATA)->data.value(HW_INIT_STEP).dataArray;
			break;
		default:
			break;
		}
	}
	if(pll2) {
		switch (pll2->getHardwareType()) {
		case hardwareDevice::LMX2326:
			pll2array = deviceParser::getDeviceList().value(hardwareDevice::PLL2)->devicePins.value(lmx2326::PIN_DATA)->data.value(HW_INIT_STEP).dataArray;
			break;
		default:
			break;
		}
	}
	if(pll3) {
		switch (pll3->getHardwareType()) {
		case hardwareDevice::LMX2326:
			pll3array = deviceParser::getDeviceList().value(hardwareDevice::PLL3)->devicePins.value(lmx2326::PIN_DATA)->data.value(HW_INIT_STEP).dataArray;
			break;
		default:
			break;
		}
	}
	if(dds3) {
		switch (dds3->getHardwareType()) {
		case hardwareDevice::AD9850:
			dds3array = deviceParser::getDeviceList().value(hardwareDevice::DDS3)->devicePins.value(ad9850::PIN_DATA)->data.value(HW_INIT_STEP).dataArray;
			break;
		default:
			break;
		}
	}
	int t = 0;
	if(pll1) {
		foreach (int step, pll1->getInitIndexes()) {
			qDebug() << "PLL1 step:" << t;
			commandInitStep(pll1, step);
			usbToString(QByteArray(), true, 0);
			++t;
		}
		t = 0;
	}
	if(dds1) {
		foreach (int step, dds1->getInitIndexes()) {
			qDebug() << "DDS1 step:" << t;
			commandInitStep(dds1, step);
			usbToString(QByteArray(), true, 0);
			++t;
		}
		t = 0;
	}
	if(pll2) {
		foreach (int step, pll2->getInitIndexes()) {
			qDebug() << "PLL2 step:" << t;
			commandInitStep(pll2, step);
			usbToString(QByteArray(), true, 0);
			++t;
		}
		t = 0;
	}
	if(pll3) {
		foreach (int step, pll3->getInitIndexes()) {
			qDebug() << "PLL3 step:" << t;
			commandInitStep(pll3, step);
			usbToString(QByteArray(), true, 0);
			++t;
		}
		t = 0;
	}
	if(dds3) {
		foreach (int step, dds3->getInitIndexes()) {
			qDebug() << "DDS3 step:" << t;
			commandInitStep(dds3, step);
			usbToString(QByteArray(), true, 0);
			++t;
		}
	}

}

void slimusb::run()
{
	forever {
		if ( QThread::currentThread()->isInterruptionRequested() ) {
			return;
		}
		commandStep(currentStep);
		if(!isInverted)
			++currentStep;
		if(currentStep > (qint32)(numberOfSteps - 1))
			currentStep = 0;
		if(isInverted)
			--currentStep;
		if(currentStep < 0)
			currentStep = numberOfSteps -1;
	}
}

void slimusb::commandInitStep(hardwareDevice *dev, int step) {

	int maxSize = 0;
	foreach (hardwareDevice::devicePin *pin, dev->getDevicePins()) {
		if(pin->data.contains(step)) {
			if(pin->data.value(step).dataArray->size() > maxSize)
				maxSize = pin->data.value(step).dataArray->size();
			if(pin->data.value(step).dataMask->size() > maxSize)
				maxSize = pin->data.value(step).dataMask->size();
		}
	};

	int previousLatch = -1;
	int previousVirtualCLK = -1;
	int currentLatch = -1;
	int currentVirtualCLK = -1;
	QByteArray sendArray;
	for(int x = 0; x < maxSize; ++x) {
		currentLatch = -1;
		currentVirtualCLK = -1;
		foreach (hardwareDevice::devicePin *pin, dev->getDevicePins()) {
			if(pin->data.contains(step) && pin->data.value(step).dataMask && (x < pin->data.value(step).dataMask->size()) && pin->data.value(step).dataMask->at(x)) {
				if(currentLatch == -1 && pin->hwconfig) {
					currentLatch = ((parallelEqui*)(pin->hwconfig))->latch;
					if(previousLatch == -1)
						previousLatch = currentLatch;
				} else if(pin->hwconfig && (currentLatch != ((parallelEqui*)(pin->hwconfig))->latch)){
					qDebug()<< currentLatch <<"!=" << ((parallelEqui*)(pin->hwconfig))->latch;
					Q_ASSERT(false);
				}
			}
		}
		Q_ASSERT(currentLatch != -1);
		foreach (hardwareDevice::devicePin *pin, dev->getDevicePins()) {
			if(pin->data.contains(step) && pin->data.value(step).dataMask && (x < pin->data.value(step).dataMask->size()) && pin->data.value(step).dataMask->at(x)) {
				if(pin->IOtype == hardwareDevice::VIRTUAL_CLK) {
					currentVirtualCLK = pin->data.value(step).dataArray->at(x);
					if(previousVirtualCLK == -1)
						previousVirtualCLK = currentVirtualCLK;
					//qDebug() << "setting virtual clock to:" << currentVirtualCLK << "bit:" << x;
				}
			}
		}
		if(previousVirtualCLK == -1)
			previousVirtualCLK = 0;
		if((currentLatch != previousLatch) || ((currentVirtualCLK != previousVirtualCLK) && currentVirtualCLK !=-1)) {
			//qDebug() << "currentLatch"<<currentLatch<< "previousLatch"<<previousLatch<<"currentVirtualCLK"<<currentVirtualCLK<<"previousVirtualCLK"<<previousVirtualCLK;
			if(sendArray.size() > 0) {
				sendUSB(sendArray, previousLatch, previousVirtualCLK == 1);
				sendArray.clear();
				previousLatch = currentLatch;
				previousVirtualCLK = currentVirtualCLK;
			}
		}

		uint8_t currentByte = 0;
		foreach (hardwareDevice::devicePin *pin, dev->getDevicePins()) {
			if(pin->data.contains(step) && pin->data.value(step).dataMask && (x < pin->data.value(step).dataMask->size()) && pin->data.value(step).dataMask->at(x)) {
				if(pin->hwconfig) {
					currentByte	= ((currentByte & ((parallelEqui*)(pin->hwconfig))->mask)) | (pin->data.value(step).dataArray->at(x) << ((parallelEqui*)(pin->hwconfig))->pin);
				}
			}
		}
		sendArray.append(currentByte);
	}
	if(sendArray.size() > 0) {
		sendUSB(sendArray, currentLatch, currentVirtualCLK == 1);
	}
}

void slimusb::sendUSB(QByteArray data, uint8_t latch, bool autoClock, bool isADC) {
	static int temp = 0;
	//qDebug() << temp << "send latch:" << latch << "autoclk:"<< autoClock << "ADC:" << isADC << data;
	static int z = 0;
	if(z == 2) {
		//while(true);
	}
	++z;
	if(!isADC) {
		int s = data.size();
		data.insert(0, (int)autoClock);
		data.insert(0, s);
		data.insert(0, 0xA0 + latchToUSBNumber.value(latch));
	}
	if(usb.isConnected()) {
		if(!isADC) {
			if(!usb.sendArray(data)) {
				this->requestInterruption();
			}
		}
		else {
			if(!usb.sendArray(data, usbB2union.data, expectedAdcSize)) {
				qDebug() << "There was an issue with the adc usb transfer";
			}
			else {
				emit dataReady(lastCommandedStep, usbB2union.command.adcMAG, usbB2union.command.adcPhase);
			}
		}
	}
	else
		usbToString(data, false, temp);
	++temp;
}
void slimusb::usbToString(QByteArray array, bool print, int temp) {
	QString str;
	unsigned char *data = reinterpret_cast<unsigned char*>(array.data());
	for(int i = 0; i < array.length(); ++i) {
		QString d = QString::number(data[i], 16);
		if(d.length() == 1)
			d.insert(0,"0");
		str.append(d);
	}

	//	latchToUSBNumber.insert(1,1);
	//	latchToUSBNumber.insert(2,3);
	//	latchToUSBNumber.insert(3,0);
	//	latchToUSBNumber.insert(4,2);
	static uint8_t latch1 = 0;
	static uint8_t latch2 = 0;
	static uint8_t latch3 = 0;
	static uint8_t latch4 = 0;
	QString autoClk;
	uint8_t *latch = NULL;
	static QStringList list;
	if(array.length() > 0) {
		if((array.at(0) & 0xA0) == 0xA0) {
			if((array.at(0) & 0x0F) == 0x00) {
				latch = &latch3;
			}
			else if((array.at(0) & 0x0F) == 0x01) {
				latch = &latch1;
			}
			else if((array.at(0) & 0x0F) == 0x02) {
				latch = &latch4;
			}
			else if((array.at(0) & 0x0F) == 0x03) {
				latch = &latch2;
			}
		}
		if((array.at(2)) == 0x01) {
			autoClk = "A";
		}
		else
			autoClk = "X";
		if(latch) {
			for(int x = 3; x < array.length(); ++ x) {
				*latch = array.at(x);
				list.append(QString::number(temp) + " " + constructString(latch1, latch2, latch3, latch4, autoClk));
			}
		}
	}
	if(print) {
		list.append("");
		foreach (QString s, list) {
			if(!s.isEmpty())
				qDebug() << s;
		}
		list.clear();
	}
}
QString slimusb::constructString(uint8_t latch1, uint8_t latch2, uint8_t latch3, uint8_t latch4, QString clock) {
	QString str;
	str = QString("%0 %1 %2 %3 %4").arg(clock).arg(byteToString(latch1)).arg(byteToString(latch2)).arg(byteToString(latch3)).arg(byteToString(latch4));
	return str;
}

QString slimusb::byteToString(uint8_t byte) {
	QString str;
	for(int x = 0; x < 8; ++x) {
		uint8_t v = ((byte) >> x) & 1;
		str.insert(0, QString::number(v));
	}
	return str;
}

void slimusb::autoScan()
{
	this->start();
}

void slimusb::pauseScan()
{
	this->requestInterruption();
	this->wait(1000);
	isPaused = true;
}

void slimusb::resumeScan()
{
	this->start();
	isPaused = false;
}

bool slimusb::getAutoConnect() const
{
	return autoConnect;
}

void slimusb::setAutoConnect(bool value)
{
	autoConnect = value;
	usb.enableCallBack(autoConnect);
}

void slimusb::printUSBData(int step) {
	QString str;
	unsigned char *data = reinterpret_cast<unsigned char*>(usbData.value(step)->data());
	for(int i = 0; i < usbData.value(step)->length(); ++i) {
		QString d = QString::number(data[i], 16);
		if(d.length() == 1)
			d.insert(0,"0");
		str.append(d);
	}
	qDebug() << str;
}

unsigned long slimusb::getWriteReadDelay_us() const
{
	return writeReadDelay_us;
}

void slimusb::setWriteReadDelay_us(unsigned long value)
{
	writeReadDelay_us = value;
}

bool slimusb::isScanning()
{
	return this->isRunning();
}

QList<usbdevice::usbDevice_t> slimusb::getDevices()
{
	return usb.getDevices();
}

bool slimusb::openDevice(int deviceNumber)
{
	return usb.openDevice(deviceNumber);
}

void slimusb::closeDevice()
{
	usb.closeDevice();
}
