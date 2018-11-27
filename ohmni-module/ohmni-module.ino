void setup() {

  // Start connection to ohmni serial bus
  Serial.begin(115200);
  
  // Initialize example output pin. Add
  // your own initialization here.
  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);  
  
}

// Commands
#define CMD_EEP_WRITE  0x01
#define CMD_EEP_READ  0x02
#define CMD_RAM_WRITE 0x03
#define CMD_RAM_READ  0x04
#define CMD_I_JOG   0x05
#define CMD_S_JOG   0x06
#define CMD_STAT    0x07
#define CMD_ROLLBACK  0x08
#define CMD_REBOOT    0x09

// Baked in SID - change to appropriate unique
// ID for your own device
uint8_t sid = 0x50;

// Received message buffer
uint8_t cbuf[64];
int cbufpos = 0;
int cbufreq = 0;
uint8_t currcsum = 0;

// Output message buffer
uint8_t obuf[64];
int obufpos = 0;
int obuflen = 0;

// Receive state machine
enum uart_state {
  UART_STATE_SEEK1,
  UART_STATE_SEEK2,
  UART_STATE_GET_LENGTH,
  UART_STATE_READN
};
uart_state state = UART_STATE_SEEK1;

// Protocol errors
int badsumcount = 0;

// State variables (replace with your own)
uint8_t ledstate = 0;

/* Payload validation for read/write commands */
int uart_validate_payload_read()
{
  // Make sure we have both addr and size in the payload
  if (cbufreq < 6)
    return 0;

  // Okay
  return 1;
}

int uart_validate_payload_write()
{
  // Make sure we have both addr and size for payload
  if (cbufreq < 6)
    return 0;

  // Check payload length versus packet length
  // to make sure our bytes are readable
  uint8_t plen = cbuf[5];
  if (cbufreq < 6 + plen) {
    return 0;
  }

  // Okay
  return 1;
}

/* Helper for read commands to build reply buffer */
void uart_build_reply(void *rdata, int rlen)
{
  // Abort if too long
  if (rlen + 10 > sizeof(obuf)) return;

  uint8_t *req = cbuf;
  uint8_t *b = obuf;

  // Preamble
  *b++ = 0xff;
  *b++ = 0xff;
  
  // Size, SID, and message type with reply bit
  int msglen = 9 + rlen + 2;
  *b++ = msglen;
  *b++ = req[0];
  *b++ = req[1] | 0x40;

  // Zero checksums for now
  *b++ = 0;
  *b++ = 0;

  // Address copied over, use our reply len
  *b++ = req[4];
  *b++ = rlen;

  // Copy over reply data
  for (int i = 0; i < rlen; i++) {
    *b++ = ((uint8_t*)rdata)[i];
  }

  // Status error and detail
  *b++ = 0x00;
  *b++ = 0x00;

  // Compute checksum now on OUTPUT buffer
  uint8_t cs = 0x00;
  for (int i = 2; i < msglen; i++) {
    cs ^= obuf[i];
  }

  // Write the checksums here to OUTPUT buffer
  obuf[5] = cs & 0xfe;
  obuf[6] = ~cs & 0xfe;

  // Start the reply now
  obufpos = 0;
  obuflen = msglen;

}

void uart_handle_cmd_ram_write()
{
  uint8_t *b = cbuf;
  uint8_t addr = b[4];
  uint8_t plen = b[5];
  uint8_t *data = b + 6;

  switch(addr) {

    // Handle writing LED as test param
    case 53:
      if (plen == 1) {
        // Set state and gpio
        ledstate = data[0];        
        digitalWrite(13, ledstate ? HIGH : LOW); 
      }
      break;
   
  }
}

void uart_handle_cmd_ram_read()
{
  uint8_t *b = cbuf;
  uint8_t addr = b[4];
  uint8_t plen = b[5];
  uint8_t *data = b + 6;

  switch(addr) {

    // Handle reading LED as a test param to set and get
    case 53:
      if (plen == 1) {        
        uart_build_reply(&ledstate, 1); 
      }
      break;

  }
}

/* Handle completion of a variable read of bytes */
void uart_handle_read_complete()
{
  // Pointer for convenience
  uint8_t *b = cbuf;

  // Validate checksum
  if ((currcsum & 0xfe) != b[2] ||
    (~currcsum & 0xfe) != b[3]) {
    badsumcount++;
    return;
  }

  // Check if this message is for us
  if (b[0] != sid && b[0] != 0xfe) {
    return;
  }

  // Okay, message for us, handle here
  switch(b[1]) {

    case CMD_EEP_WRITE:
      if (!uart_validate_payload_write()) return;
      //uart_handle_cmd_eep_write();
      break;    

    case CMD_EEP_READ:
      if (!uart_validate_payload_read()) return;
      //uart_handle_cmd_eep_read();
      break;

    case CMD_RAM_WRITE:
      if (!uart_validate_payload_write()) return;
      uart_handle_cmd_ram_write();
      break;

    case CMD_RAM_READ:
      if (!uart_validate_payload_read()) return;
      uart_handle_cmd_ram_read();
      break;

    case CMD_REBOOT:
      //uart_handle_cmd_reboot();
      break;

  }

}

/* Byte by byte handler */
void uart_handle_byte(uint8_t b)
{
  // Now handle state machine
  switch(state) {

    case UART_STATE_SEEK1: 
    {
      state = b == 0xff ? UART_STATE_SEEK2 : UART_STATE_SEEK1;     
      break;
    }

    case UART_STATE_SEEK2:
    {
      state = b == 0xff ? UART_STATE_GET_LENGTH : UART_STATE_SEEK1;
      break;
    }

    case UART_STATE_GET_LENGTH:
    {
      /* Check reasonable bounds on desired length */
      if (b < 7 || b > 64) {
        state = UART_STATE_SEEK1;
        return;
      }

      /* Set this as readn */
      cbufpos = 0;
      cbufreq = b-3;     
      state = UART_STATE_READN;     

      /* Start running checksum */
      currcsum = b;

      break;
    }

    case UART_STATE_READN:
    {     
      // Update checksum EXCEPT for csum bytes
      if (cbufpos != 2 && cbufpos != 3) 
        currcsum ^= b;

      // Store the new byte
      cbuf[cbufpos++] = b;

      // Check for finish
      if (cbufpos >= cbufreq) {

        // Call the completion here
        uart_handle_read_complete();

        // Go back to root
        state = UART_STATE_SEEK1;     
      }

      break;
    }
  }
}

// Main loop
void loop() {

  // Poll for bytes to read
  if (Serial.available() > 0) {
    uint8_t inb = Serial.read();
    uart_handle_byte(inb);
  }

  // Check for bytes to write
  if (obufpos < obuflen) {
    Serial.write(obuf[obufpos++]);
  }
  
}
