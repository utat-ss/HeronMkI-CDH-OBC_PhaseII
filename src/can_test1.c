/*
	Author: Keenan Burnett
 
	***********************************************************************
	*	FILE NAME:		can_test1.c
	*
	*	PURPOSE:		
	*	This is a ported version of the CAN Example used in Atmel studio.
	*	
	*
	*	FILE REFERENCES:	can_test1.h 
	*
	*	EXTERNAL VARIABLES:
	*
	*	EXTERNAL REFERENCES:	sn65hvd234.h and can.h by extension of can_test1.h
	*
	*	ABORNOMAL TERMINATION CONDITIONS, ERROR AND WARNING MESSAGES: 
	*
	*	In Atmel's CAN example, sn65hvd234_disable() tend to run into an infinite loop,
	*	although within the main program here, there does not seem to be any issues.
	*	Simply, refrain from disabling the transceiver component and all should be fine.
	*
	*	ASSUMPTIONS, CONSTRAINTS, CONDITIONS:	None
	*
	*	NOTES:	 In future code, avoid the use of sn65hvd234_disable(), there isn't
	*	really a need for it as CAN doesn't need to be disabled once the CPU is running.
	*	
	*	REQUIREMENTS/ FUNCTIONAL SPECIFICATION REFERENCES:			
	*	New tasks should be written to use as much of CMSIS as possible. The ASF and 
	*	FreeRTOS API libraries should also be used whenever possible to make the program
	*	more portable.

	*	DEVELOPMENT HISTORY:		
	*	12/20/2014		Created.
	*
 */
#include "can_test1.h"

volatile uint32_t g_ul_recv_status = 0;

/**
 * \brief Default interrupt handler for CAN 1.
 */
void CAN1_Handler(void)
{
	uint32_t ul_status;

	ul_status = can_get_status(CAN1);
	if (ul_status & GLOBAL_MAILBOX_MASK) {
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) {
			ul_status = can_mailbox_get_status(CAN1, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) {
				can1_mailbox.ul_mb_idx = i;
				can1_mailbox.ul_status = ul_status;
				can_mailbox_read(CAN1, &can1_mailbox);
				g_ul_recv_status = 1;
				
				// Decode CAN Message
				decode_can_msg(&can1_mailbox, CAN1);
				//assert(g_ul_recv_status); ***Implement assert here.
				g_ul_recv_status = 0;
				break;
			}
		}
	}
}
/************************************************************************/
/* Default Interrupt Handler for CAN0								    */
/************************************************************************/

/**
 * \brief Default interrupt handler for CAN 0.
 */
void CAN0_Handler(void)
{
	uint32_t ul_status;

	ul_status = can_get_status(CAN0);
	if (ul_status & GLOBAL_MAILBOX_MASK) {
		for (uint8_t i = 0; i < CANMB_NUMBER; i++) {
			ul_status = can_mailbox_get_status(CAN0, i);
			
			if ((ul_status & CAN_MSR_MRDY) == CAN_MSR_MRDY) {
				can0_mailbox.ul_mb_idx = i;
				can0_mailbox.ul_status = ul_status;
				can_mailbox_read(CAN0, &can0_mailbox);
				g_ul_recv_status = 1;
				
				// Decode CAN Message
				decode_can_msg(&can0_mailbox, CAN0);
				//assert(g_ul_recv_status); ***implement assert here.
				break;
			}
		}
	}
}

/************************************************************************/
/* Decode CAN Message													*/
/* Performs a prescribed action depending on the message received       */
/************************************************************************/

/**
 * \brief Decoodes the CAN message and performs a prescribed action depending on 
 * the message received.
 * @param *controller:  	CAN sending controller
 * @param *p_mailbox:		CAN mailbox structure of sending controller
 */
void decode_can_msg(can_mb_conf_t *p_mailbox, Can* controller)
{
	//assert(g_ul_recv_status);		// Only decode if a message was received.	***Asserts here.
	//assert(controller);				// CAN0 or CAN1 are nonzero.
	uint32_t ul_data_incom = p_mailbox->ul_datal;
	if(controller == CAN0)
		pio_toggle_pin(LED0_GPIO);
	else
		pio_toggle_pin(LED1_GPIO);
	if (ul_data_incom == COMMAND_OUT)
		pio_toggle_pin(LED0_GPIO);
	if (ul_data_incom == COMMAND_IN)
		pio_toggle_pin(LED1_GPIO);

	else if ((ul_data_incom == COMMAND_IN) & (controller == CAN0)) 
	{
		// Command has been received, respond.
		pio_toggle_pin(LED0_GPIO);
		command_in();
	}
	else if ((ul_data_incom == COMMAND_OUT) & (controller == CAN1))
	{
		pio_toggle_pin(LED2_GPIO);	// LED2 indicates the response to the command
	}								// has been received.
	else if ((ul_data_incom == HK_TRANSMIT) & (controller == CAN1))
	{
		pio_toggle_pin(LED3_GPIO);	// LED3 indicates housekeeping has been received.
	}
	return;
}

/************************************************************************/
/* RESET MAILBOX CONFIGURE STRUCTURE                                    */
/************************************************************************/

/**
 * \brief Resets the mailbox configure structure.  
 * @param *p_mailbox:		Mailbox structure that will be reset. 
 */
void reset_mailbox_conf(can_mb_conf_t *p_mailbox)
{
	p_mailbox->ul_mb_idx = 0;
	p_mailbox->uc_obj_type = 0;
	p_mailbox->uc_id_ver = 0;
	p_mailbox->uc_length = 0;
	p_mailbox->uc_tx_prio = 0;
	p_mailbox->ul_status = 0;
	p_mailbox->ul_id_msk = 0;
	p_mailbox->ul_id = 0;
	p_mailbox->ul_fid = 0;
	p_mailbox->ul_datal = 0;
	p_mailbox->ul_datah = 0;
}

/************************************************************************/
/*                 SEND A 'COMMAND' FROM CAN0 TO CAN1		            */
/************************************************************************/

/**
 * \brief Sends a 'command' from CAN0 to CAN1
 */
void command_out(void)
{
	pio_toggle_pin(LED0_GPIO);
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN0 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 0;
	can0_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can0_mailbox.ul_id_msk = CAN_MAM_MIDvA_Msk | CAN_MAM_MIDvB_Msk;
	can0_mailbox.ul_id = CAN_MID_MIDvA(7);
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Init CAN1 Mailbox 0 to Transmit Mailbox. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 0;
	can1_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can1_mailbox.uc_tx_prio = 15;
	can1_mailbox.uc_id_ver = 0;
	can1_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Write transmit information into mailbox. */
	can1_mailbox.ul_id = CAN_MID_MIDvA(7);
	can1_mailbox.ul_datal = COMMAND_IN;
	can1_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can1_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN1, &can1_mailbox);

	/* Enable CAN1 mailbox 0 interrupt. */
	can_enable_interrupt(CAN0, CAN_IER_MB0);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN1, CAN_TCR_MB0);

	/* potentially @non-terminating@ */
	while (!g_ul_recv_status) {
	}

}

/************************************************************************/
/*				RESPOND TO THE COMMAND FROM CAN0 AND SEND TO CAN1       */
/************************************************************************/

/**
 * \brief Responds to he command from CAN0 and sends to CAN1
 **/
void command_in(void)
{
	pio_toggle_pin(LED0_GPIO);
	
	can_disable_interrupt(CAN0, CAN_IER_MB0);
	NVIC_DisableIRQ(CAN0_IRQn);
	
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN1 Mailbox 0 to Reception Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can1_mailbox.ul_mb_idx = 1;
	can1_mailbox.uc_obj_type = CAN_MB_RX_MODE;
	can1_mailbox.ul_id_msk = CAN_MAM_MIDvA_Msk | CAN_MAM_MIDvB_Msk;
	can1_mailbox.ul_id = CAN_MID_MIDvA(7);
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Init CAN0 Mailbox 0 to Transmit Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 1;
	can0_mailbox.uc_obj_type = CAN_MB_TX_MODE;
	can0_mailbox.uc_tx_prio = 15;
	can0_mailbox.uc_id_ver = 0;
	can0_mailbox.ul_id_msk = 0;
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Write transmit information into mailbox. */
	can0_mailbox.ul_id = CAN_MID_MIDvA(7);
	can0_mailbox.ul_datal = COMMAND_OUT;
	can0_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN0, &can0_mailbox);

	/* Enable CAN1 mailbox 0 interrupt. */
	can_enable_interrupt(CAN1, CAN_IER_MB1);

	/* Send out the information in the mailbox. */
	can_global_send_transfer_cmd(CAN0, CAN_TCR_MB1);
	/* potentially @non-terminating@ */
	while (!g_ul_recv_status) {
	}
}

/************************************************************************/
/*			THIS FUNCTION CHECKS THE FUNCTIONALITY OF THE				*/
/*			PRODUCER-CONSUMER MODEL TO BE USED FOR HOUSEKEEPING         */
/************************************************************************/

/**
 * Tests the functionality of the producer-consumer model
 * to be used for housekeeping.
 */
void test_2(void)
{
	can_reset_all_mailbox(CAN0);
	can_reset_all_mailbox(CAN1);

	/* Init CAN0 Mailbox 3 to Producer Mailbox. */
	reset_mailbox_conf(&can0_mailbox);
	can0_mailbox.ul_mb_idx = 3;				// Mailbox 3
	can0_mailbox.uc_obj_type = CAN_MB_PRODUCER_MODE;
	can0_mailbox.ul_id_msk = 0;
	can0_mailbox.ul_id = CAN_MID_MIDvA(NODE0_ID);
	can_mailbox_init(CAN0, &can0_mailbox);

	/* Prepare the response information when it receives a remote frame. */
	can0_mailbox.ul_datal = HK_TRANSMIT;
	can0_mailbox.ul_datah = CAN_MSG_DUMMY_DATA;
	can0_mailbox.uc_length = MAX_CAN_FRAME_DATA_LEN;
	can_mailbox_write(CAN0, &can0_mailbox);

	can_global_send_transfer_cmd(CAN0, CAN_TCR_MB3);

	/* Init CAN1 Mailbox 3 to Consumer Mailbox. It sends a remote frame and waits for an answer. */
	reset_mailbox_conf(&can1_mailbox);
	can1_mailbox.ul_mb_idx = 3;				// Mailbox 3
	can1_mailbox.uc_obj_type = CAN_MB_CONSUMER_MODE;
	can1_mailbox.uc_tx_prio = 9;
	can1_mailbox.ul_id_msk = CAN_MID_MIDvA_Msk | CAN_MID_MIDvB_Msk;
	can1_mailbox.ul_id = CAN_MID_MIDvA(NODE0_ID);
	can_mailbox_init(CAN1, &can1_mailbox);

	/* Enable CAN1 mailbox 3 interrupt. */
	can_enable_interrupt(CAN1, CAN_IER_MB3);

	can_global_send_transfer_cmd(CAN1, CAN_TCR_MB3);
	/* potentially @non-terminating@ */
	while (!g_ul_recv_status) {
	}
}

/**
 * \brief Initializes CAN0 and CAN1, then runs test_2. CAN0 and CAN1
 * are disabled afterwards.
 */
int can_test1(void)
{
	uint32_t ul_sysclk;

	/* Initialize CAN0 Transceiver. */
	sn65hvd234_set_rs(&can0_transceiver, PIN_CAN0_TR_RS_IDX);
	sn65hvd234_set_en(&can0_transceiver, PIN_CAN0_TR_EN_IDX);
	/* Enable CAN0 Transceiver. */
	sn65hvd234_disable_low_power(&can0_transceiver);
	sn65hvd234_enable(&can0_transceiver);

	/* Initialize CAN1 Transceiver. */
	sn65hvd234_set_rs(&can1_transceiver, PIN_CAN1_TR_RS_IDX);
	sn65hvd234_set_en(&can1_transceiver, PIN_CAN1_TR_EN_IDX);
	/* Enable CAN1 Transceiver. */
	sn65hvd234_disable_low_power(&can1_transceiver);
	sn65hvd234_enable(&can1_transceiver);

	/* Enable CAN0 & CAN1 clock. */
	pmc_enable_periph_clk(ID_CAN0);
	pmc_enable_periph_clk(ID_CAN1);

	ul_sysclk = sysclk_get_cpu_hz();
	if (can_init(CAN0, ul_sysclk, CAN_BPS_250K) &&
	can_init(CAN1, ul_sysclk, CAN_BPS_250K)) {

	/* Disable all CAN0 & CAN1 interrupts. */
	can_disable_interrupt(CAN0, CAN_DISABLE_ALL_INTERRUPT_MASK);
	can_disable_interrupt(CAN1, CAN_DISABLE_ALL_INTERRUPT_MASK);
	
	NVIC_EnableIRQ(CAN0_IRQn);
	NVIC_EnableIRQ(CAN1_IRQn);

	/* Run tests. */
	
	/* Command-out causes a 'command' to be sent from CAN0 to CAN1 */
	/* A response is then sent from CAN1 to CAN0 */
	
	//command_out();
	
	/* test_2 checks the producer-consumer model that will be used for HK */
	test_2();		
	
	/* Disable CAN0 Controller */
	can_disable(CAN0);
	/* Disable CAN0 Transceiver */
	sn65hvd234_enable_low_power(&can0_transceiver);
	sn65hvd234_disable(&can0_transceiver);

	/* Disable CAN1 Controller */
	can_disable(CAN1);
	/* Disable CAN1 Transceiver */
	sn65hvd234_enable_low_power(&can1_transceiver);
	sn65hvd234_disable(&can1_transceiver);
	} 
	
	else { }

	/* @non-terminating@ */
	while (1) {	}
	return 0;
}

