#ifndef __S5PRP_IOCTL_H__
#define __S5PRP_IOCTL_H__

#ifdef __cplusplus
extern "C" {
#endif


#define S5P_RP_INIT				(0x10000)
#define S5P_RP_DEINIT				(0x10001)

#define S5P_RP_PAUSE				(0x20000)
#define S5P_RP_STOP				(0x20001)
#define S5P_RP_FLUSH				(0x20002)
#define S5P_RP_WAIT_EOS				(0x20003)
#define S5P_RP_EFFECT				(0x20004)

#define S5P_RP_PENDING_STATE			(0x30000)
#define S5P_RP_ERROR_STATE			(0x30001)
#define S5P_RP_DECODED_FRAME_NO			(0x30002)
#define S5P_RP_DECODED_1FRAME_SIZE		(0x30003)
#define S5P_RP_DECODED_PCM_SIZE			(0x30004)


#define S5P_RP_CTRL_SET_GAIN			(0xFF000)
#define S5P_RP_CTRL_SET_EFFECT			(0xFF001)
#define S5P_RP_CTRL_GET_PCM_1KFRAME		(0xFF002)
#define S5P_RP_CTRL_PCM_DUMP_OP			(0xFF003)

#define S5P_RP_CTRL_EFFECT_ENABLE		(0xFF010)
#define S5P_RP_CTRL_EFFECT_DEF			(0xFF011)
#define S5P_RP_CTRL_EFFECT_EQ_USR		(0xFF012)
#define S5P_RP_CTRL_EFFECT_SPEAKER		(0xFF013)

#define S5P_RP_CTRL_IS_RUNNING			(0xFF100)
#define S5P_RP_CTRL_IS_OPENED			(0xFF101)
#define S5P_RP_CTRL_GET_OP_LEVEL		(0xFF102)

#define S5P_RP_CTRL_ALTFW_STATE			(0xFF200)
#define S5P_RP_CTRL_ALTFW_LOAD			(0xFF201)

#define S5P_RP_FW_CODE1				0
#define S5P_RP_FW_CODE20			1
#define S5P_RP_FW_CODE21			2
#define S5P_RP_FW_CODE22			3
#define S5P_RP_FW_CODE30			4
#define S5P_RP_FW_CODE31			5

/* Sound Alive */
#define SA_BYPASS				0x00
#define SA_3D					0x01
#define SA_REVERB				0x02
#define SA_MC					0x03
#define SA_BE					0x04
#define SA_SE					0x05
#define SA_SUROUND				0x06

#ifdef __cplusplus
}
#endif

#endif /* __S5PRP_IOCTL_H__ */

