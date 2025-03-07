    recvBuf[n] = '\0'; 
    printf("%s", recvBuf); 
 
    if (n >= 129) 
      shiftRows(&recvBuf[128], rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1); 
 
    if (n>= 65){ 
      recvBuf[128] = '\0'; 
      shiftRows(&recvBuf[64], rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1); 
    } 
 
    recvBuf[64] = '\0'; 
    shiftRows(recvBuf, rowBuf, (sizeof(rowBuf)/sizeof(rowBuf[1]))-1); 
 
    // memset(recvBuf, 0, BUFFER_SIZE); 
  } 
 
  for (int i=1; i<8; i++){ 
    free(rowBuf[i]); 
  } 
 
  return NULL; 
}
-- VISUAL --                                                  600,1         Bot

