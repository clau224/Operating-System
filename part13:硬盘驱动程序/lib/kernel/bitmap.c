#include "bitmap.h"
#include "stdint.h"
#include "string.h"
#include "print.h"
#include "interrupt.h"
#include "debug.h"


/*初始化位图*/
void bitmap_init(struct bitmap* btmp){
	memset(btmp->bits, 0, btmp->btmp_bytes_len);
}

/*判断目标位是否为1，是的话返回true*/
bool bitmap_scan_test(struct bitmap* btmp, uint32_t bit_idx){
	uint32_t byte_idx = bit_idx/8;
	uint32_t bit_odd = bit_idx % 8;
	return (btmp->bits[byte_idx]) & (0x01<<bit_odd); 
}

/*在位图中申请不间断的cnt个位，成功返回起始位下标，失败返回-1*/
int bitmap_scan(struct bitmap* btmp, uint32_t cnt){
	uint32_t idx_byte = 0;
	//先找到第一个不是0xff的字节
	while(btmp->bits[idx_byte] == 0xff && idx_byte < btmp->btmp_bytes_len)
		idx_byte++;
	ASSERT(idx_byte < btmp->btmp_bytes_len);
	//假如该字节是bitmap最后一个字节，说明bitmap已经全满
	if(idx_byte == btmp->btmp_bytes_len)
		return -1;

	int idx_bit = 0;
	//在刚刚找到的字节中，找出第一个不为1的位
	while((uint8_t)(0x01 << idx_bit) & btmp->bits[idx_byte]){
		idx_bit++;
	}
	
	int bit_idx_start = idx_byte*8 + idx_bit;
	//如果只需申请一个位，就可以返回了
	if(cnt == 1)
		return bit_idx_start;
	//记录还剩多少位
	uint32_t bit_left = (btmp->btmp_bytes_len*8 - bit_idx_start);
	//即将要去检查的位
	uint32_t next_bit = bit_idx_start+1;
	uint32_t count = 1;

	bit_idx_start = -1;
	while(bit_left-- > 0){
		//如果该bit位是0，则为空，count+1
		if(!(bitmap_scan_test(btmp, next_bit)))
			count++;
		//否则，被截断了，count置0
		else
			count = 0;
		//找到了目标数量需要的空位，可以返回空位的开始位置
		if(count == cnt){
			bit_idx_start = next_bit - cnt + 1;
			break;		
		}	
		next_bit++;
	}
	return bit_idx_start;
}

/*将位图btmp的第bit_idx位设置为value*/
void bitmap_set(struct bitmap* btmp, uint32_t bit_idx, int8_t value){
	ASSERT(value == 0 || value == 1);
	uint32_t byte_idx = bit_idx/8;
	uint32_t bit_odd = bit_idx % 8;
	if(value){
		btmp->bits[byte_idx] = btmp->bits[byte_idx] | 0x01 << bit_odd;	
	}
	else{
		btmp->bits[byte_idx] = btmp->bits[byte_idx] & ~(0x01 << bit_odd);
	}
}


