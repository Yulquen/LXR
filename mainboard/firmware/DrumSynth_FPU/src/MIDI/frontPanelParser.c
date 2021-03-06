/*
 * frontPanelParser.c
 *
 *  Created on: 27.04.2012
 * ------------------------------------------------------------------------------------------------------------------------
 *  Copyright 2013 Julian Schmidt
 *  Julian@sonic-potions.com
 * ------------------------------------------------------------------------------------------------------------------------
 *  This file is part of the Sonic Potions LXR drumsynth firmware.
 * ------------------------------------------------------------------------------------------------------------------------
 *  Redistribution and use of the LXR code or any derivative works are permitted
 *  provided that the following conditions are met:
 *
 *       - The code may not be sold, nor may it be used in a commercial product or activity.
 *
 *       - Redistributions that are modified from the original source must include the complete
 *         source code, including the source code for all components used by a binary built
 *         from the modified sources. However, as a special exception, the source code distributed
 *         need not include anything that is normally distributed (in either source or binary form)
 *         with the major components (compiler, kernel, and so on) of the operating system on which
 *         the executable runs, unless that component itself accompanies the executable.
 *
 *       - Redistributions must reproduce the above copyright notice, this list of conditions and the
 *         following disclaimer in the documentation and/or other materials provided with the distribution.
 * ------------------------------------------------------------------------------------------------------------------------
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 *   INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *   DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *   SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *   WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 *   USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * ------------------------------------------------------------------------------------------------------------------------
 */



#include "frontPanelParser.h"
#include "MidiMessages.h"
#include "MidiParser.h"
#include "sequencer.h"
#include "Uart.h"
#include "SD_Manager.h"
#include "EuklidGenerator.h"
#include "config.h"
#include "mixer.h"

#include "DrumVoice.h"
#include "CymbalVoice.h"
#include "HiHat.h"
#include "Snare.h"
#include "SomGenerator.h"



//a counter for the received bytes
//each message is made up from 3 bytes (status 0xb0, parameter nr and parameter value)
 uint8_t frontParser_rxCnt=0;

//the currently selected parameter
 MidiMsg frontParser_midiMsg;

//the currently active track on the fron panel (shown on the seq led buttons)
 uint8_t frontParser_activeFrontTrack=0;

//used to store the sysex mode and to block all other uart messages while sysexActive != 0
 uint8_t frontParser_sysexActive=0;
/** used to collect 2 7 bit messages and combine them to a 14 bit message*/
 uint16_t frontParser_twoByteData=0;

 uint8_t frontParser_sysexBuffer[7];

/** used to count incoming sequencer step data packages*/
 uint16_t frontParser_sysexSeqStepNr=0;

 uint8_t frontParser_activeTrack=0;	/** the active track on the Frontpanel. track specific messages refer to the track selected with this command*/
uint8_t frontParser_shownPattern = 0;
 uint8_t frontParser_activeStep=0;

//------------------------------------------------------
/**send all active step numbers to frontpanel to light up corresponding LEDs*/
void frontParser_updateTrackLeds(const uint8_t trackNr, uint8_t patternNr)
{
	if(trackNr<=6)
	{

		frontParser_activeFrontTrack = trackNr;

		int i=0;

		for(;i<16;i++)
		{
			if(seq_isMainStepActive(trackNr,i,patternNr))
			{
				uart_sendFrontpanelByte(FRONT_STEP_LED_STATUS_BYTE);
				uart_sendFrontpanelByte(FRONT_LED_SEQ_BUTTON);
				uart_sendFrontpanelByte(i*8);
			}
		}

		uint8_t start = (frontParser_activeStep/8)*8;
		for(i=start;i<(start+8);i++) //only send visible substeps
		{
			if(seq_isStepActive(trackNr,i,patternNr))
			{
				uart_sendFrontpanelByte(FRONT_STEP_LED_STATUS_BYTE);
				uart_sendFrontpanelByte(FRONT_LED_SEQ_SUB_STEP);
				uart_sendFrontpanelByte(i);
			}
		}
	}
}
//------------------------------------------------------
void frontParser_parseUartData(unsigned char data)
{

	//TODO der ganze sysex kram kann sicher noch optimiert werden
	//das das nicht andauernd abgefragt werden muss

	// if high byte set a new message starts
	if(data&0x80)
	{
		//reset the byte counter
		frontParser_rxCnt = 0;
		frontParser_midiMsg.status = data;
		if(data==SYSEX_START)
		{
			frontParser_sysexActive = SYSEX_ACTIVE_MODE_NONE;
			uart_clearFrontFifo();

			//send SYSEX_START as ACK
			uart_sendFrontpanelSysExByte(SYSEX_START);
		}
		else
		{
			frontParser_sysexActive = SYSEX_INACTIVE;
		}
	}
	else
	{
		if(frontParser_midiMsg.status == SYSEX_START)
		{
//SYSEX
			// this is not a correct sysex implementation.
			// in the front panel communication it is used to send sequencer data for preset saving
			// while the preset saving is active, no other data has to be send over the uart!!!
			// so when the SYSEX_START is received, all other communication is stopped until theSYSEX_END message is received
			// no abort and no manufacturer id is supported

			{
				//we have received sysex data
				//first we receive a mode message
				//SYSEX_REQUEST_STEP_DATA or SYSEX_SEND_STEP_DATA
				//then the corresponding data

				switch(frontParser_sysexActive)
				{
				default:
				case SYSEX_ACTIVE_MODE_NONE:
					//we received a mode message -> set mode
					frontParser_sysexActive = data;
					frontParser_sysexSeqStepNr = 0;
					frontParser_rxCnt = 0;

					break;

				case SYSEX_REQUEST_PATTERN_DATA:
					//1 byte = pattern nr
					//send back next and repeat
					uart_sendFrontpanelSysExByte(seq_patternSet.seq_patternSettings[data].nextPattern);
					uart_sendFrontpanelSysExByte(seq_patternSet.seq_patternSettings[data].changeBar);
					break;

				case SYSEX_REQUEST_MAIN_STEP_DATA:
					//we expect a 2 byte message containing a step nr
					//which tells us which main step data to send back
					frontParser_rxCnt++;
					if(frontParser_rxCnt == 1)
					{
						//first nibble received -> upper nibble
						frontParser_twoByteData = data<<7;
					}
					else
					{
						//second nibble -> lower nibble
						frontParser_twoByteData |= data&0x7f;
						//reset rxCount for next 2 nibble message
						frontParser_rxCnt = 0;
						//we have received a complete 2 nibble step nr
						//send data back to front
						seq_sendMainStepInfoToFront(frontParser_twoByteData);
					}
				break;

				case SYSEX_REQUEST_STEP_DATA:
					//we expect a 2 byte message containing a step nr
					//which tells us which step data to send back
					frontParser_rxCnt++;

					if(frontParser_rxCnt == 1)
					{
						//first nibble received -> upper nibble
						frontParser_twoByteData = data<<7;
					}
					else
					{
						//second nibble -> lower nibble
						frontParser_twoByteData |= data&0x7f;
						//reset rxCount for next 2 nibble message
						frontParser_rxCnt = 0;
						//we have received a complete 2 nibble step nr
						//send data back to front
						seq_sendStepInfoToFront(frontParser_twoByteData);
					}

					break;

				case SYSEX_RECEIVE_MAIN_STEP_DATA:
					if(frontParser_rxCnt<2)
					{
						frontParser_sysexBuffer[frontParser_rxCnt++] = data;
					}
					else
					{
						frontParser_sysexBuffer[frontParser_rxCnt++] = data;

						//calculate the step pattern and track indices
						const uint8_t currentPattern	= frontParser_sysexSeqStepNr / 7;
						const uint8_t currentTrack  	= frontParser_sysexSeqStepNr - currentPattern*7;

						uint16_t mainStepData = frontParser_sysexBuffer[0] | frontParser_sysexBuffer[1]<<7 | frontParser_sysexBuffer[2]<<14;

						//first load into inactive track
						PatternSet* patternSet = &seq_patternSet;

						if( (currentPattern == seq_activePattern) && seq_isRunning() )
						{
							seq_tmpPattern.seq_mainSteps[currentTrack] = mainStepData;
						} else {
							patternSet->seq_mainSteps[currentPattern][currentTrack] = mainStepData;
						}


						//inc the step counter
						frontParser_sysexSeqStepNr++;
						frontParser_rxCnt = 0;

						//signal new pattern
						if( seq_isRunning()) {
							seq_newPatternAvailable = 1;
						}
					}
					break;

				case SYSEX_RECEIVE_STEP_DATA:
					// we expect a bunch of 8 byte sysex message containing new step data for the sequencer
					// beginning with step 0 up to NUMBER_STEPS*NUM_TRACKS*NUM_PATTERN = 128*7*8 = 7168 steps
					if(frontParser_rxCnt<7)
					{
						frontParser_sysexBuffer[frontParser_rxCnt++] = data;
					}
					else
					{
						//now we have to distribute the MSBs to the sysex data
						uint8_t i;
						for(i=0;i<7;i++)
						{
							frontParser_sysexBuffer[i] |= ((data&(1<<i))<<(7-i));

						}

						//calculate the step pattern and track indices
						const uint8_t absPat 	= frontParser_sysexSeqStepNr/128;
						const uint8_t currentTrack 		= absPat / 8;
						const uint8_t currentPattern 	= absPat - currentTrack*8;
						const uint8_t currentStep		= frontParser_sysexSeqStepNr - absPat*128;

						PatternSet* patternSet = &seq_patternSet;

						//do not overwrite playing pattern
						if( (currentPattern == seq_activePattern)   && seq_isRunning())
						{
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].volume 	= frontParser_sysexBuffer[0];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].prob 	= frontParser_sysexBuffer[1];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].note 	= frontParser_sysexBuffer[2];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].param1Nr = frontParser_sysexBuffer[3];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].param1Val 	= frontParser_sysexBuffer[4];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].param2Nr 	= frontParser_sysexBuffer[5];
							seq_tmpPattern.seq_subStepPattern[currentTrack][currentStep].param2Val 	= frontParser_sysexBuffer[6];
						} else {

							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].volume 	= frontParser_sysexBuffer[0];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].prob 	= frontParser_sysexBuffer[1];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].note 	= frontParser_sysexBuffer[2];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].param1Nr = frontParser_sysexBuffer[3];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].param1Val 	= frontParser_sysexBuffer[4];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].param2Nr 	= frontParser_sysexBuffer[5];
							patternSet->seq_subStepPattern[currentPattern][currentTrack][currentStep].param2Val 	= frontParser_sysexBuffer[6];
						}
						//signal that a new data chunk is available
						//frontParser_newSeqDataAvailable = 1;
						//reset receive counter for next chunk
						frontParser_rxCnt = 0;

						//inc the step counter
						frontParser_sysexSeqStepNr++;
					}

					break;
				}
			}
		}

//normal operation
		else if(frontParser_rxCnt==0)
		{
			//parameter nr
			frontParser_midiMsg.data1 = data;
			frontParser_rxCnt++;
		}
		else
		{
			//parameter value
			frontParser_midiMsg.data2 = data;
			//process the midi message

			switch(frontParser_midiMsg.status)
			{

			case SAMPLE_CC:
				switch(frontParser_midiMsg.data1)
				{

				case FRONT_SAMPLE_START_UPLOAD:
					seq_setRunning(0);
					sampleMemory_init();
					sampleMemory_loadSamples();
					FLASH_Lock();

					uart_sendFrontpanelByte(ACK);
				break;

				case FRONT_SAMPLE_COUNT:
					uart_sendFrontpanelByte(SAMPLE_CC);
					uart_sendFrontpanelByte(FRONT_SAMPLE_COUNT);
					uart_sendFrontpanelByte(sampleMemory_getNumSamples());
					break;

				default:
					break;
				}
			break;

//MIDI SYNTH MESSAGES
			case MIDI_CC:

			//correct parameter number offset
			frontParser_midiMsg.data1 += 1;

			//fix offset between front an cortex
			// front params start at 1, cortex at 2 (because of midi in mod wheel==0x1

			//because hh slope on front is 127 and on cortex is 0 wrap data1 at 127
			frontParser_midiMsg.data1 &= 0x7f;

			midiParser_ccHandler(frontParser_midiMsg,1);

			//record automation if record is turned on
			seq_recordAutomation(frontParser_activeTrack, frontParser_midiMsg.data1, frontParser_midiMsg.data2);
			break;


			case FRONT_CC_LFO_TARGET:
			{
				uint8_t upper = frontParser_midiMsg.data1;
				uint8_t lower = frontParser_midiMsg.data2;
				uint8_t value = ((upper&0x01)<<7) | lower;
				uint8_t lfoNr = (upper&0xfe)>>1;

				switch(lfoNr)
				{
				case 0:
				case 1:
				case 2:
					modNode_setDestination(&voiceArray[lfoNr].lfo.modTarget, value);
					break;

				case 3:
					modNode_setDestination(&snareVoice.lfo.modTarget,value);
					break;

				case 4:
					modNode_setDestination(&cymbalVoice.lfo.modTarget, value);
					break;

				case 5:
					modNode_setDestination(&hatVoice.lfo.modTarget, value);
					break;

				default:
					break;
				}


			}
			break;

			case FRONT_SET_P1_DEST: {
					uint8_t hi = frontParser_midiMsg.data1;
					uint8_t lo = frontParser_midiMsg.data2;
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][seq_selectedStep].param1Nr = ((hi<<7)|lo);
				}
				break;
			case FRONT_SET_P2_DEST: {
					uint8_t hi = frontParser_midiMsg.data1;
					uint8_t lo = frontParser_midiMsg.data2;
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][seq_selectedStep].param2Nr = (hi<<7)|lo;
				}
				break;
			case FRONT_SET_P1_VAL: {
					uint8_t stepNr = frontParser_midiMsg.data1;
					uint8_t value = frontParser_midiMsg.data2;
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][stepNr].param1Val = value;

				}
				break;
			case FRONT_SET_P2_VAL: {
					uint8_t stepNr = frontParser_midiMsg.data1;
					uint8_t value = frontParser_midiMsg.data2;
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][stepNr].param2Val = value;
				}
				break;

			case FRONT_ARM_AUTOMATION_STEP:
				{
					const uint8_t stepNr 	= frontParser_midiMsg.data1;
					const uint8_t onOff 	= frontParser_midiMsg.data2 & 0x40;
					const uint8_t trackNr 	=  frontParser_midiMsg.data2 & 0x3f;

					seq_armAutomationStep(stepNr,trackNr,onOff);

				}
				break;



			case FRONT_MAIN_STEP_CC:
				{
					//data 1 = track und pattern nr
					//data 2 = step nr
					uint8_t voiceNr 	= frontParser_midiMsg.data1 >> 4;
					uint8_t patternNr 	= frontParser_midiMsg.data1 & 0x7;
					uint8_t stepNr 		= frontParser_midiMsg.data2;


					//toggle the step in the seq
					seq_toggleMainStep(voiceNr, stepNr, patternNr);

					//if step active send led on message to front
					if(seq_isMainStepActive(voiceNr, stepNr, patternNr))
					{
						uart_sendFrontpanelByte(FRONT_STEP_LED_STATUS_BYTE);
						uart_sendFrontpanelByte(FRONT_LED_SEQ_BUTTON);
						uart_sendFrontpanelByte(stepNr*8);
					}
				}
				break;

			case FRONT_STEP_CC:
				{
					//data 1 = track und pattern nr
					//data 2 = step nr
					uint8_t voiceNr 	= frontParser_midiMsg.data1 >> 4;
					uint8_t patternNr 	= frontParser_midiMsg.data1 & 0x7;
					uint8_t stepNr 		= frontParser_midiMsg.data2;

					//toggle the step in the seq
					seq_toggleStep(voiceNr, stepNr, patternNr);

				}
				break;

			case FRONT_CC_VELO_TARGET:
				{
					uint8_t upper = frontParser_midiMsg.data1;
					uint8_t lower = frontParser_midiMsg.data2;
					uint8_t value = ((upper&0x01)<<7) | lower;
					uint8_t velModNr = (upper&0xfe)>>1;
					modNode_setDestination(&velocityModulators[velModNr], value);
				}
				break;
//CC2 above 127

			case FRONT_CC_2:
				{
					midiParser_ccHandler(frontParser_midiMsg,1);

					//record automation if record is turned on
					seq_recordAutomation(frontParser_activeTrack, frontParser_midiMsg.data1+128, frontParser_midiMsg.data2);
				}
				break;

			//VOICE option Messages
			case VOICE_CC:
				break;

			//BPM MESSAGE
			case FRONT_SET_BPM: {
					uint16_t bpm = frontParser_midiMsg.data1 |(uint16_t)(frontParser_midiMsg.data2<<7);
					if(bpm == 0) {
						seq_setExtSync(1);
					}
					else {
						seq_setBpm(bpm);

						seq_setExtSync(0);
					}
				}
				break;
			//SEQ MESSAGES
			case FRONT_SEQ_CC:
				switch(frontParser_midiMsg.data1)
				{

				case FRONT_SEQ_REQUEST_PATTERN_PARAMS:
					/* send back bar change and next pattern params from requested pattern*/
					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_SET_PAT_BEAT);
					uart_sendFrontpanelByte(seq_patternSet.seq_patternSettings[frontParser_shownPattern].changeBar);

					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_SET_PAT_NEXT);
					uart_sendFrontpanelByte(seq_patternSet.seq_patternSettings[frontParser_shownPattern].nextPattern);


					break;
				case FRONT_SEQ_SET_PAT_BEAT:
					seq_patternSet.seq_patternSettings[frontParser_shownPattern].changeBar = frontParser_midiMsg.data2;
					break;
				case FRONT_SEQ_SET_PAT_NEXT:
					seq_patternSet.seq_patternSettings[frontParser_shownPattern].nextPattern = frontParser_midiMsg.data2;
					break;

				case FRONT_SEQ_REC_ON_OFF:
					seq_setRecordingMode(frontParser_midiMsg.data2);
					break;


				case FRONT_SEQ_NOTE:
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_activeStep].note = frontParser_midiMsg.data2;
					break;

				case FRONT_SEQ_VOLUME:
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_activeStep].volume &= ~(0x7f);
					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_activeStep].volume |= (frontParser_midiMsg.data2&0x7f);
					break;

				case FRONT_SEQ_PROB:

					seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_activeStep].prob = frontParser_midiMsg.data2;
					break;

				case FRONT_SEQ_EUKLID_LENGTH:
					{
						uint8_t length 	= frontParser_midiMsg.data2 >> 3;
						length += 1;
						uint8_t pattern = frontParser_midiMsg.data2 & 0x7;

						{
							euklid_setLength(frontParser_activeTrack, length, pattern);
						}
					}
					break;

				case FRONT_SEQ_EUKLID_STEPS:
					{
						uint8_t steps 	= frontParser_midiMsg.data2 >> 3;
						steps += 1;
						uint8_t pattern = frontParser_midiMsg.data2 & 0x7;

						euklid_setSteps(frontParser_activeTrack,steps,pattern);
						frontParser_updateTrackLeds(frontParser_activeTrack, pattern);
					}
					break;

				case FRONT_SEQ_CLEAR_TRACK: {
						seq_clearTrack(frontParser_midiMsg.data2, frontParser_shownPattern);
					}
					break;

				case FRONT_SEQ_CLEAR_PATTERN:
					 seq_clearPattern(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_POSX:
					som_setX(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_POSY:
					som_setY(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_FLUX:
					som_setFlux(frontParser_midiMsg.data2/127.f);
					break;

				case FRONT_SEQ_SOM_FREQ:
					som_setFreq(frontParser_midiMsg.data2,frontParser_activeTrack);
					break;

				case FRONT_SEQ_MIDI_CHAN:
				{
					uint8_t voice = frontParser_midiMsg.data2 >> 4;
					uint8_t channel = frontParser_midiMsg.data2 & 0x0f;
					midi_MidiChannels[voice] = channel;
				}
					break;

				case FRONT_SEQ_MIDI_MODE:
					midi_mode = frontParser_midiMsg.data2;
					break;


				//voice nr (0xf0) + autom track nr (0x0f)
				case FRONT_SEQ_CLEAR_AUTOM:
				{
					const uint8_t voice 		= frontParser_midiMsg.data2 >> 4;
					const uint8_t automTrack 	= frontParser_midiMsg.data2 &  0x0f;
					seq_clearAutomation(voice, frontParser_shownPattern, automTrack);
				}
					break;

				case FRONT_SEQ_COPY_TRACK:
					{
						const uint8_t src = frontParser_midiMsg.data2>>4;
						const uint8_t dst = frontParser_midiMsg.data2&0xf;
						seq_copyTrack(src,dst,frontParser_shownPattern);
					}
					break;

				case FRONT_SEQ_COPY_PATTERN:
					{
						const uint8_t src = frontParser_midiMsg.data2>>4;
						const uint8_t dst = frontParser_midiMsg.data2&0xf;
						seq_copyPattern(src,dst);
					}
					break;



				case FRONT_SEQ_TRACK_LENGTH:
					seq_setTrackLength(frontParser_activeTrack,frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_SHUFFLE:
					seq_setShuffle(frontParser_midiMsg.data2/127.f);
					break;

				case FRONT_SEQ_SELECT_ACTIVE_STEP:
					seq_selectedStep = frontParser_midiMsg.data2;
					break;

				case FRONT_SEQ_SET_AUTOM_TRACK:
					seq_setActiveAutomationTrack(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_SET_QUANT:
						seq_setQuantisation(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_REQUEST_EUKLID_PARAMS:
					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_EUKLID_LENGTH);
					uart_sendFrontpanelByte(euklid_getLength(frontParser_midiMsg.data2));

					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_EUKLID_STEPS);
					uart_sendFrontpanelByte(euklid_getSteps(frontParser_midiMsg.data2));
					break;

				case FRONT_SEQ_SET_SHOWN_PATTERN:
					frontParser_shownPattern = frontParser_midiMsg.data2;
					break;
				case FRONT_SEQ_SET_ACTIVE_TRACK:
					frontParser_activeTrack = frontParser_midiMsg.data2;
					break;

				case FRONT_SEQ_REQUEST_STEP_PARAMS:{


					/* send back probability, volume and note nr*/
					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_VOLUME);
					uart_sendFrontpanelByte(seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].volume&STEP_VOLUME_MASK);

					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_NOTE);
					uart_sendFrontpanelByte(seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].note);

					uart_sendFrontpanelByte(FRONT_SEQ_CC);
					uart_sendFrontpanelByte(FRONT_SEQ_PROB);
					uart_sendFrontpanelByte(seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].prob);

					//send back automation params
					uint8_t hi,lo;
					uint8_t dest = seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].param1Nr;
					hi = dest>>7;
					lo = dest&0x7f;
					uart_sendFrontpanelByte(FRONT_SET_P1_DEST);
					uart_sendFrontpanelByte(hi);
					uart_sendFrontpanelByte(lo);

					uint8_t val = seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].param1Val;
					hi = val>>7;
					lo = val&0x7f;
					uart_sendFrontpanelByte(FRONT_SET_P1_VAL);
					uart_sendFrontpanelByte(hi);
					uart_sendFrontpanelByte(lo);

					dest = seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].param2Nr;
					hi = dest>>7;
					lo = dest&0x7f;
					uart_sendFrontpanelByte(FRONT_SET_P2_DEST);
					uart_sendFrontpanelByte(hi);
					uart_sendFrontpanelByte(lo);

					val = seq_patternSet.seq_subStepPattern[frontParser_shownPattern][frontParser_activeTrack][frontParser_midiMsg.data2].param2Val;
					hi = val>>7;
					lo = val&0x7f;
					uart_sendFrontpanelByte(FRONT_SET_P2_VAL);
					uart_sendFrontpanelByte(hi);
					uart_sendFrontpanelByte(lo);



					frontParser_activeStep = frontParser_midiMsg.data2;
				}
					break;

				case FRONT_SEQ_ROLL_RATE:
					seq_setRollRate(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_ROLL_ON_OFF:
				{

					const uint8_t onOff = frontParser_midiMsg.data2 >> 4;
					const uint8_t voice = frontParser_midiMsg.data2 & 0xf;
					seq_setRoll(voice,onOff);
				}
					break;

				case FRONT_SEQ_CHANGE_PAT:
					//switch to one of the 8 patterns on the next pattern start
					seq_setNextPattern(frontParser_midiMsg.data2&0x7);
					break;


				case FRONT_SEQ_RUN_STOP:
					seq_setRunning(frontParser_midiMsg.data2);
					break;

				case FRONT_SEQ_MUTE_TRACK:
					seq_setMute(frontParser_midiMsg.data2,1);
					break;

				case FRONT_SEQ_UNMUTE_TRACK:
					seq_setMute(frontParser_midiMsg.data2,0);
					break;

				default:
					break;
				}
				break;


			//LED MESSAGES
			case FRONT_STEP_LED_STATUS_BYTE:
				switch(frontParser_midiMsg.data1)
				{
					//send all active step numbers to frontpanel to light up corresponding LEDs
					case FRONT_LED_QUERY_SEQ_TRACK:
					{
						uint8_t trackNr = frontParser_midiMsg.data2 >> 4;
						uint8_t patternNr = frontParser_midiMsg.data2 & 0x7;

						frontParser_updateTrackLeds(trackNr, patternNr);

						//send track length back
						uart_sendFrontpanelByte(FRONT_SEQ_CC);
						uart_sendFrontpanelByte(FRONT_SEQ_TRACK_LENGTH);
						uart_sendFrontpanelByte(seq_getTrackLength(trackNr));
					}
					break;

				default:
					break;
				}
				break;
			}
		}
	}
};
//------------------------------------------------------
