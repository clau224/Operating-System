#include "list.h"
#include "interrupt.h"

void list_init(struct list* list){
	list->head.prev = NULL;
	list->head.next = &(list->tail);
	list->tail.next = NULL;
	list->tail.prev = &(list->head);
} 

//将链表元素elem插入在元素before前
void list_insert_before(struct list_elem* before, struct list_elem* elem){
	enum intr_status old_status = intr_disable();

	elem->prev = before->prev;
	elem->next = before->next;
	before->prev->next = elem;
	before->prev = elem;

	intr_set_status(old_status);
}

//添加元素到链表首
void list_push(struct list* l, struct list_elem* e){
	list_insert_before(l->head.next, e);
}

//添加元素到链表尾
void list_append(struct list* l, struct list_elem* e){
	list_insert_before(&l->tail, e);
}


//使元素e脱离链表
void list_remove(struct list_elem* e){
	enum intr_status old_status = intr_disable();
	e->prev->next = e->next;
	e->next->prev = e->prev;
	intr_set_status(old_status);
}

//从链表首弹出元素
struct list_elem* list_pop(struct list* l){
	struct list_elem* ans = l->head.next;
	list_remove(ans);
	return ans;
}

//从链表l中查找target元素
bool elem_find(struct list* l, struct list_elem* target){
	struct list_elem* item = l->head.next;
	while(item!= &(l->tail)){
		if(item == target)
			return true;
		item = item->next;	
	}
	return false;
}

//返回链表长度
uint32_t list_len(struct list* l){
	struct list_elem* item = l->head.next;
	uint32_t length = 0;
	while(item != &l->tail){
		length++;
		item = item->next;	
	}
	return length;
}

//返回链表是否为空
bool list_empty(struct list* l){
	return l->head.next == &l->tail;
}

//逐个判断是否满足条件，其中判定依据是func(elem, arg)
struct list_elem* list_traversal(struct list* l, function func, int arg){
	struct list_elem* item = l->head.next;
	if(list_empty(l))
		return NULL;
	while(item != &(l->tail)){
		if(func(item, arg)){
			return item;
		}
		item = item->next;
	}
	return NULL;
}










