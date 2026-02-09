
/**************************************************************************************************************/
/************************************** Функции работы с буфером 16 бит ***************************************/
/**************************************************************************************************************/
/*
 * Очистить очередь слов данных (16 бит)
 */
void Clear_queue_word(FIFO_RING_BUFFER_WORD *queue_val)
{
  queue_val->back = 0;
  queue_val->font = 0;
  queue_val->count = 0;
  //for(uint16_t i=0;i<SIZE_FIFO_BUF_WORD;i++) queue_val->buf[i]=0;
}

/*
 * Положить символ в очередь  слов данных (16 бит)
 * функция возвращает ноль если очередь полная и единицу если очередь еще заполнена не до конца
 */
uint16_t PutChar_word(FIFO_RING_BUFFER_WORD *queue_val,const canFrame frame)
{
  queue_val->dat[queue_val->font].id = frame.id;              //помещаем в очередь символ
  queue_val->dat[queue_val->font].rtr = frame.rtr;
  queue_val->dat[queue_val->font].dlc = frame.dlc;
  for(int i = 0; i < frame.dlc; i++)
    queue_val->dat[queue_val->font].dat[i] = frame.dat[i];  
      
  queue_val->back++;                       // инкрементируем указатель конечного элемента очереди
  if(queue_val->back >= SIZE_FIFO_BUF_WORD) queue_val->back = 0; // если указатель конца очереди вышел за пределы буфера то сбрасываем его на начало массива очереди

  if (queue_val->count < SIZE_FIFO_BUF_WORD) // если в очереди есть место
    {
    queue_val->count++;                    //инкрементируем счетчик символов
    if(queue_val->count == SIZE_FIFO_BUF_WORD) return 0;
    else return 1;
  }
  else // если очередь заполнена
  {
    queue_val->font = queue_val->back; //
    return 0;
  }
}

/*
 * Взять символ из очереди слов данных (16 бит)
 */
canFrame GetChar_word(FIFO_RING_BUFFER_WORD *queue_val)
{
  canFrame frame;
  frame.id = 0;
  frame.rtr = 0;
  frame.dlc = 0;
  if (queue_val->count > 0)  //если очередь не пустая
  {
    frame.id = queue_val->dat[queue_val->font].id;              //считываем данные из очереди
    frame.rtr = queue_val->dat[queue_val->font].rtr;
    frame.dlc = queue_val->dat[queue_val->font].dlc;
    for(int i = 0; i < frame.dlc; i++)
      frame.dat[i] = queue_val->dat[queue_val->font].dat[i];  
    
    queue_val->count--;                                   //уменьшаем счетчик символов
    queue_val->font++;                                  //инкрементируем индекс головы буфера
    if (queue_val->font == SIZE_FIFO_BUF_WORD) queue_val->font = 0;
  }
  return frame;
}

/*----------------------------------------------------------------------------------------------------------------*/
