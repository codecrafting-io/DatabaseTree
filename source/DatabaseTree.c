//============================================================================
// Name        : DatabaseTree.c
// Author      : Lucas Lara Marotta
// Description : BPlus Tree Implementation based on book File Organization - Alan Tharp
// College     : UFBA - Universidade Federal da Bahia
// Email       : lucas.marotta@dcc.ufba.br
// License	   : See LICENSE file
//============================================================================

#include "TreeHeader.h"


/*____________ FUNÇÕES AUXILIARES ____________*/

//Função que verifica ou cria o arquivo de nós de Ìndice
FILE* checkFileIndex(void)
{
	FILE *f;
	f = fopen("IndexTree.cw","rb+");
	if(f) return f;
	else return fopen("IndexTree.cw", "wb+");
}

//Função que verifica ou cria o arquivo de nós Terminais
FILE* checkFileTerminal(void)
{
	FILE *f;
	f = fopen("TerminalNodes.cw","rb+");
	if(f) return f;
	else return fopen("TerminalNodes.cw", "wb+");
}

//Função que retorna o número de registros presente no arquivo
int numberRegisters(FILE* f,char nodetype)
{
	fseek(f,0,SEEK_END);
	//Tamanho do Registro de Índices
	if(nodetype == 'i')
	{
		return ftell(f)/sizeof(IndexNode);
	}
	return ftell(f)/sizeof(TerminalNode);
}

//Função que realiza uma busca binária num array de Registros
int buscaBinaria(Register *array,int key)
{
    int inf = 0; //Limite inferior
    int sup = REGS_POR_PAGINA - 1; //Limite superior
    int meio;
    while (inf <= sup)
    {
         meio = inf + (sup-inf)/2;
         if (key == array[meio].key)
              return meio;
         else if (key < array[meio].key || array[meio].key == 0)
              sup = meio-1;
         else
              inf = meio+1;
    }
    return -1;   // não encontrado
}


/*____________ FUNÇÕES PARA NÓS DE ÍNDICE ____________*/

_Bool pushStack(StackTop *stack,IndexNode *inode)
{
	IndexStack *node;
	node = (IndexStack *) malloc(sizeof(IndexStack));

	if(!node) return false;

	node->indexNode = *inode;
	node->next = stack->top;
	stack->top = node;
	return true;
}

IndexNode* popStack(StackTop *stack)
{

	if(stack->top)
	{
		IndexStack *node = (IndexStack*) malloc(sizeof(IndexNode));
		IndexNode *inode = (IndexNode*) malloc(sizeof(IndexNode));

		node = stack->top;
		*inode = node->indexNode;
		stack->top = stack->top->next;
		free(node);
		return inode;
	}
	return NULL;
}

void initIndexNode(IndexNode *inode)
{
	int i;
	inode->qtdeKeys = 0;
	inode->leaf = true;
	for(i=0;i < 2*ORDEM;i++)
	{
		inode->keys[i] = 0;
		inode->pointers[i] = 0;
	}
	inode->pointers[i] = 0;
}

void shiftPointers(int *pointers,int pos,int pos_val)
{
	int aux,shift_val,i;

	aux = pos_val;
	for(i=pos;i < (2*ORDEM)+1 && pointers[i] != 0; i++)
	{
		shift_val = pointers[i];
		pointers[i] = aux;
		aux = shift_val;
	}
	if(i < (2*ORDEM)+1) pointers[i] = aux;
}

void createSplitIndex(StackTop *istack,int nreg,int promotedKey,FILE *fi)
{
	int i,offset;
	IndexNode *inode_stack,split_inode;

	inode_stack = (IndexNode*) malloc(sizeof(IndexNode));
	inode_stack = popStack(istack);

	while(inode_stack && inode_stack->qtdeKeys == 2*ORDEM)
	{
		initIndexNode(&split_inode);
		promotedKey = splitIndex(inode_stack,promotedKey,&split_inode,nreg);
		nreg = numberRegisters(fi,'i');
		//Apesar de istack foi atualizado o idNode ainda não mudou. Assim continuo sabendo se o nó de split foi o da raiz
		if(inode_stack->idNode == 1)
		{
			inode_stack->idNode = nreg+1;
			offset = sizeof(IndexNode) * nreg;
			fseek(fi,offset,SEEK_SET);
			fwrite(inode_stack,sizeof(IndexNode),1,fi);
			split_inode.idNode = nreg+2;
			offset = sizeof(IndexNode) * (nreg+1);
			fseek(fi,offset,SEEK_SET);
			fwrite(&split_inode,sizeof(IndexNode),1,fi);
		}

		else
		{
			offset = sizeof(IndexNode) * (inode_stack->idNode - 1);
			fseek(fi,offset,SEEK_SET);
			fwrite(inode_stack,sizeof(IndexNode),1,fi);
			split_inode.idNode = nreg + 1;
			fseek(fi,0,SEEK_END);
			fwrite(&split_inode,sizeof(IndexNode),1,fi);
			nreg++;
		}
		inode_stack = popStack(istack);
	}

	//promotedKey é a chave promovida. Insere a chave promovida no nó e este não está cheio
	if(inode_stack)
	{
		i = inserirOrdenadoInode(inode_stack->keys,promotedKey) + 1;
		shiftPointers(inode_stack->pointers,i,nreg);
		inode_stack->qtdeKeys += 1;

		offset = sizeof(IndexNode) * (inode_stack->idNode - 1);
		fseek(fi,offset,SEEK_SET);
		fwrite(inode_stack,sizeof(IndexNode),1,fi);
	}

	//Uma nova raiz será criada
	else
	{
		initIndexNode(&split_inode);
		inserirOrdenadoInode(split_inode.keys,promotedKey);
		split_inode.pointers[0] = nreg + 1;
		split_inode.pointers[1] = nreg + 2;
		split_inode.qtdeKeys = 1;
		split_inode.idNode = 1;
		split_inode.leaf = false;
		fseek(fi,0,0);
		fwrite(&split_inode,sizeof(IndexNode),1,fi);
	}


}

int splitIndex(IndexNode *istack,int key, IndexNode *split_inode,int nreg)
{
	int i,v,index,promotedKey,middle;
	IndexNode inode_aux;

	inode_aux = *istack;
	index = (2*ORDEM + 1) / 2;
	middle = inode_aux.keys[index];
	split_inode->leaf = istack->leaf;
	if(inode_aux.pointers[0] < 0) nreg = nreg * -1;

	v = 0;
	if(key < middle)
	{
		for(i=(index-1);i < 2*ORDEM;i++)
		{
			istack->keys[i] = 0;
			istack->pointers[i+1] = 0;
			istack->qtdeKeys--;
		}

		if(key < inode_aux.keys[index-1])
		{
			promotedKey = inode_aux.keys[index-1];
			split_inode->pointers[v] = inode_aux.pointers[index];
			i = inserirOrdenadoInode(istack->keys,key) + 1;
			shiftPointers(istack->pointers,i,nreg);
			istack->qtdeKeys++;
		}

		else
		{
			promotedKey = key;
			split_inode->pointers[v] = nreg;
			istack->keys[index-1] = inode_aux.keys[index-1];
			istack->pointers[index] = inode_aux.pointers[index];
			istack->qtdeKeys++;
		}

		for(i=index; i < (2*ORDEM);i++,v++)
		{
			split_inode->keys[v] = inode_aux.keys[i];
			split_inode->pointers[v+1] = inode_aux.pointers[i+1];
			split_inode->qtdeKeys++;
		}
	}

	else
	{
		promotedKey = middle;
		for(i=index;i < 2*ORDEM;i++)
		{
			istack->keys[i] = 0;
			istack->pointers[i+1] = 0;
			istack->qtdeKeys--;
		}

		for(i=index+1;i < 2*ORDEM;i++,v++)
		{
			split_inode->keys[v] = inode_aux.keys[i];
			split_inode->pointers[v] = inode_aux.pointers[i];
			split_inode->qtdeKeys++;
		}
		split_inode->pointers[v] = inode_aux.pointers[i];
		i = inserirOrdenadoInode(split_inode->keys,key) + 1;
		split_inode->qtdeKeys++;
		shiftPointers(split_inode->pointers,i,nreg);
	}

	return promotedKey;
}

_Bool enQueueIndex(IQueue *queue,IndexNode *inode)
{
	IndexQueue *node = (IndexQueue*) malloc(sizeof(IndexQueue));
	if(!node) return false;
	node->indexNode = *inode;
	node->next = NULL;
	if(queue->first == NULL) queue->first = node;
	else queue->last->next = node;
	queue->last = node;
	return true;
}

IndexNode* deQueueIndex(IQueue *queue)
{
	if(queue->first)
	{
		IndexQueue *node = (IndexQueue*) malloc(sizeof(IndexQueue));
		IndexNode *inode = (IndexNode*) malloc(sizeof(IndexNode));

		node = queue->first;
		*inode = node->indexNode;
		queue->first = node->next;
		if(queue->first == NULL) queue->last = queue->first;
		free(node);
		return inode;
	}
	return NULL;
}

int inserirOrdenadoInode(int *index_keys,int key)
{
	int i,v;
	int aux,anterior;
	for(i = 0;i < 2*ORDEM;i++)
	{
		if(key < index_keys[i] || index_keys[i] == 0)
			break;
	}

	if(index_keys[i] == 0)
	{
		index_keys[i] = key;
	}

	else
	{
		anterior = key;
		for(v = i;v < 2*ORDEM;v++)
		{
			aux = index_keys[v];
			index_keys[v] = anterior;
			anterior = aux;
		}
	}
	return i;
}

void removeOrdenadoInode(IndexNode *inode, int pos)
{
	int i;
	for(i = pos; i < 2*ORDEM;i++)
	{
		(*inode).keys[i] = (*inode).keys[i+1];
		(*inode).pointers[i] = (*inode).pointers[i+1];
	}
	(*inode).keys[i] = 0;
	(*inode).pointers[i] = 0;
}


/*____________ FUNÇÕES PARA NÓS TERMINAIS ____________*/

_Bool enQueueTerminal(TQueue *queue,TerminalNode *tnode)
{
	TerminalQueue *node = (TerminalQueue*) malloc(sizeof(TerminalQueue));
	if(!node) return false;
	node->terminalNode = *tnode;
	node->next = NULL;
	if(queue->first == NULL) queue->first = node;
	else queue->last->next = node;
	queue->last = node;
	return true;
}

TerminalNode* deQueueTerminal(TQueue *queue)
{
	if(queue->first)
	{
		TerminalQueue *node = (TerminalQueue*) malloc(sizeof(TerminalQueue));
		TerminalNode *tnode = (TerminalNode*) malloc(sizeof(TerminalNode));

		node = queue->first;
		*tnode = node->terminalNode;
		queue->first = node->next;
		if(queue->first == NULL) queue->last = queue->first;
		free(node);
		return tnode;
	}
	return NULL;
}

void initTerminalNode(TerminalNode *tnode)
{
	int i;

	tnode->qtdeReg = 0;
	tnode->next_node = 0;
	for(i=0;i < REGS_POR_PAGINA;i++)
	{
		tnode->reg[i].age = 0;
		tnode->reg[i].key = 0;
	}
}

TerminalNode splitTerminal(Register *set_keys,Register *reg,int *qtdeReg)
{
	int i = 0,middle;
	TerminalNode tnode;

	middle = (REGS_POR_PAGINA + 1)/2;
	tnode.qtdeReg = 0;

	//Inicializa Terminal Node
	initTerminalNode(&tnode);

	//O meio de fato é o middle
	if((*reg).key > set_keys[middle].key)
	{
		for(i = middle+1;i < REGS_POR_PAGINA;i++)
		{
			inserirOrdenadoTnode(tnode.reg,&set_keys[i]);
			tnode.qtdeReg++;
			set_keys[i].key = 0;

			*qtdeReg -= 1;
		}
		inserirOrdenadoTnode(tnode.reg,reg);
		tnode.qtdeReg++;
	}

	else
	{
		//Tira os excedentes para um novo nó em regs
		for(i = middle;i < REGS_POR_PAGINA;i++)
		{
			inserirOrdenadoTnode(tnode.reg,&set_keys[i]);
			set_keys[i].key = 0;
			*qtdeReg -= 1;
			tnode.qtdeReg++;
		}

		//O meio é o próprio reg
		if((*reg).key > set_keys[middle-1].key) set_keys[middle] = *reg;

		//O meio é o middle - 1
		else inserirOrdenadoTnode(set_keys,reg);

		*qtdeReg += 1;
	}

	return tnode;
}

void inserirOrdenadoTnode(Register *set_keys,Register *reg)
{
	int i,v;
	Register aux,anterior;
	for(i = 0;i < REGS_POR_PAGINA;i++)
	{
		if((*reg).key < set_keys[i].key || set_keys[i].key == 0)
			break;
	}

	if(set_keys[i].key == 0)
	{
		 set_keys[i].key = (*reg).key;
		 strcpy(set_keys[i].name,(*reg).name);
		 set_keys[i].age = (*reg).age;
	}

	else
	{
		anterior = *reg;
		for(v = i;v < REGS_POR_PAGINA;v++)
		{
			aux = set_keys[v];
			set_keys[v] = anterior;
			anterior = aux;
		}
	}
}


/*____________ DEFINIÇÕES DE FUNÇÕES PRINCIPAIS ____________*/

_Bool insereRegistro(FILE *fi,FILE *ft,Register *reg)
{
	int i,offset,busca,nreg,page;
	IndexNode inode;
	TerminalNode tnode,tnode_aux;
	StackTop stack;

	stack.top = NULL;
	page = fread(&inode,sizeof(IndexNode),1,fi);

	//Arquivo vazio - Inserir Raiz.
	if(page != 1)
	{
		//Inicializa Index Node
		initIndexNode(&inode);
		inode.idNode = 1;
		inode.keys[0] = (*reg).key;
		inode.pointers[0] = -1;
		inode.leaf = true;
		inode.qtdeKeys = 1;
		fwrite(&inode,sizeof(IndexNode),1,fi);

		//Inicializa Terminal Node
		initTerminalNode(&tnode);
		tnode.reg[0] = *reg;
		tnode.qtdeReg = 1;
		tnode.next_node = 0;

		fwrite(&tnode,sizeof(TerminalNode),1,ft);
	}

	//Há registros
	else
	{
		while(!inode.leaf)
		{
			for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
			{
				if((*reg).key <= inode.keys[i]) break;
			}

			//Coloca na pilha
			pushStack(&stack,&inode);
			offset = sizeof(IndexNode) * (inode.pointers[i] - 1);
			fseek(fi,offset,SEEK_SET);
			fread(&inode,sizeof(IndexNode),1,fi);
		}

		for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
		{
			if((*reg).key <= inode.keys[i]) break;
		}

		//Nó terminal não existe
		if(inode.pointers[i] == 0)
		{
			//Inicializa Terminal Node - uso nreg só pra não criar uma nova variável de controle
			initTerminalNode(&tnode);
			tnode.reg[0] = *reg;
			tnode.qtdeReg = 1;
			nreg = numberRegisters(ft,'t');
			inode.pointers[i] = (nreg+1) * -1;
			offset = sizeof(IndexNode) * (inode.idNode - 1);

			//Atualiza o nó de índice
			fseek(fi,offset,SEEK_SET);
			fwrite(&inode,sizeof(IndexNode),1,fi);
			offset = sizeof(TerminalNode) * ((inode.pointers[i-1] * -1) - 1);

			//tnode_aux é o terminal anterior ao ser inserido
			fseek(ft,offset,SEEK_SET);
			fread(&tnode_aux,sizeof(TerminalNode),1,ft);
			tnode.next_node = tnode_aux.next_node;
			tnode_aux.next_node = nreg + 1;

			//Atualizar nó anterior
			offset = sizeof(TerminalNode) * ((inode.pointers[i-1] * -1) - 1);
			fseek(ft,offset,SEEK_SET);
			fwrite(&tnode_aux,sizeof(TerminalNode),1,ft);

			//Gravar o novo nó
			fseek(ft,0,SEEK_END);
			fwrite(&tnode,sizeof(TerminalNode),1,ft);
			return true;
		}

		//Nó Terminal Já existente
		else
		{
			offset = sizeof(TerminalNode) * ((inode.pointers[i] * -1)-1);
			fseek(ft,offset,SEEK_SET);
			fread(&tnode,sizeof(TerminalNode),1,ft);
			busca = buscaBinaria(tnode.reg,(*reg).key);

			//Insiro normalmente
			if(busca == -1 && tnode.qtdeReg < REGS_POR_PAGINA)
			{
				inserirOrdenadoTnode(tnode.reg,reg);
				tnode.qtdeReg++;
				fseek(ft,offset,SEEK_SET);
				fwrite(&tnode,sizeof(TerminalNode),1,ft);
				return true;
			}

			//Chave já existente
			else if(busca != -1) return false;

			//Caso de split
			else
			{
				tnode_aux = splitTerminal(tnode.reg,reg,&tnode.qtdeReg);
				nreg = numberRegisters(ft,'t');

				//Insere o novo nó gerado pelo split
				tnode_aux.next_node = tnode.next_node;
				fseek(ft,0,SEEK_END);
				fwrite(&tnode_aux,sizeof(TerminalNode),1,ft);

				//Atualiza o nó que ocorreu o overflow
				tnode.next_node = nreg+1;
				offset = sizeof(TerminalNode) * ((inode.pointers[i] * -1) - 1);
				fseek(ft,offset,SEEK_SET);
				fwrite(&tnode,sizeof(TerminalNode),1,ft);

				//Split apenas no nó terminal
				if(inode.qtdeKeys < 2*ORDEM)
				{
					//Atualizar o nó do índice
					inserirOrdenadoInode(inode.keys,tnode.reg[tnode.qtdeReg - 1].key);
					shiftPointers(inode.pointers,i+1,(nreg+1) * -1);

					inode.qtdeKeys++;
					offset = sizeof(IndexNode) * (inode.idNode - 1);
					fseek(fi,offset,SEEK_SET);
					fwrite(&inode,sizeof(IndexNode),1,fi);
				}

				//split no nó de índice
				else
				{
					//busca é a chave promovida do split
					pushStack(&stack,&inode);
					busca = tnode.reg[tnode.qtdeReg - 1].key;
					createSplitIndex(&stack,nreg+1,busca,fi);
				}
			}
		}
	}

	return true;
}

Register* slowQuery(FILE *ft,int key)
{
	int offset,page,nreg,busca = -1;
	TerminalNode tnode;
	Register *reg_aux = malloc(sizeof(Register));

	fseek(ft,0,SEEK_SET);
	page = fread(&tnode,sizeof(TerminalNode),1,ft);

	if(page == 1)
	{
		nreg = numberRegisters(ft,'t');
		page = 0;

		while(page < nreg)
		{
			busca = buscaBinaria(tnode.reg,key);
			if(busca != -1)
			{
				*reg_aux = tnode.reg[busca];
				return reg_aux;
			}

			if(tnode.next_node != 0)
			{
				offset = sizeof(TerminalNode) * (tnode.next_node - 1);
				fseek(ft,offset,SEEK_SET);
				fread(&tnode,sizeof(TerminalNode),1,ft);
			}
			page++;
		}
	}
	return NULL;
}


Register* consultaRegistro(FILE *fi,FILE *ft,int key)
{
	int i,offset,busca,page;
	IndexNode inode;
	TerminalNode tnode;
	Register *reg_aux = malloc(sizeof(Register));

	page = fread(&inode,sizeof(IndexNode),1,fi);

	if(page == 1)
	{
		while(!inode.leaf)
		{
			for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
			{
				if(key <= inode.keys[i]) break;
			}
			offset = sizeof(IndexNode) * (inode.pointers[i] - 1);
			fseek(fi,offset,SEEK_SET);
			fread(&inode,sizeof(IndexNode),1,fi);
		}

		for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
		{
			if(key <= inode.keys[i]) break;
		}

		if(inode.pointers[i] != 0)
		{
			offset = sizeof(TerminalNode) * ((inode.pointers[i] * -1)-1);
			fseek(ft,offset,SEEK_SET);
			fread(&tnode,sizeof(TerminalNode),1,ft);
			busca = buscaBinaria(tnode.reg,key);
			if(busca != -1)
			{
				*reg_aux = tnode.reg[busca];
				return reg_aux;
			}
		}
	}
	return NULL;
}

void imprimeNosTerminais(FILE *ft)
{
	int i,v,offset,page,nreg;
	TerminalNode tnode;

	page = fread(&tnode,sizeof(TerminalNode),1,ft);

	v = 1;
	if(page == 1)
	{
		nreg = numberRegisters(ft,'t');
		page = 0;

		while(page < nreg)
		{
			printf("\nNo: %d",v);
			for(i=0;i < tnode.qtdeReg;i++)
			{
				printf("\n%d\n%s\n%d\n",tnode.reg[i].key,tnode.reg[i].name,tnode.reg[i].age);
			}
			v++;


			if(tnode.next_node != 0)
			{
				offset = sizeof(TerminalNode) * (tnode.next_node - 1);
				fseek(ft,offset,SEEK_SET);
				fread(&tnode,sizeof(TerminalNode),1,ft);
			}
			page++;
		}
	}
}

void imprimeArvore(FILE *fi,FILE *ft)
{
	int i,v,k,offset,page;
	IndexNode inode,*inode_queue;
	TerminalNode *tnode_queue,tnode;
	IQueue queueIndex;
	TQueue queueTerminal;

	queueIndex.first = queueIndex.last = NULL;
	queueTerminal.first = queueTerminal.last = NULL;

	page = fread(&inode,sizeof(IndexNode),1,fi);
	v = 1;
	k = 1;

	if(page==1)
	{
		enQueueIndex(&queueIndex,&inode);
		while(queueIndex.first)
		{
			inode_queue = deQueueIndex(&queueIndex);
			printf("No: %d: ",v);
			for(i=0;inode_queue->pointers[i] != 0 && i < (2*ORDEM)+1;i++)
			{
				printf("apontador: %d ",k+1);
				k++;
				if(!inode_queue->leaf)
				{
					offset = sizeof(IndexNode) * (inode_queue->pointers[i] - 1);
					fseek(fi,offset,SEEK_SET);
					fread(&inode,sizeof(IndexNode),1,fi);
					enQueueIndex(&queueIndex,&inode);
				}
			}
			v++;

			for(i=0;i < inode_queue->qtdeKeys;i++)
			{
				printf("chave: %d ",inode_queue->keys[i]);
			}

			if(inode_queue->leaf)
			{
				for(i=0;inode_queue->pointers[i] != 0 && i < (2*ORDEM)+1;i++)
				{
					offset = sizeof(TerminalNode) * ((inode_queue->pointers[i] * -1) - 1);
					fseek(ft,offset,SEEK_SET);
					fread(&tnode,sizeof(TerminalNode),1,ft);
					if(tnode.qtdeReg > 0) enQueueTerminal(&queueTerminal,&tnode);
				}
			}
			printf("\n");
		}

		k = 0;
		while(queueTerminal.first)
		{
			tnode_queue = deQueueTerminal(&queueTerminal);
			printf("No: %d: ",v);
			for(i=0;i < tnode_queue->qtdeReg;i++)
			{
				printf("chave: %d ",tnode_queue->reg[i].key);
				k++;
			}
			v++;
			printf("\n");
		}
		printf("%d chaves\n",k);
	}
}

int removeOrdenadoTnode(Register *set_keys,int key)
{
	int pos,i;
	pos = buscaBinaria(set_keys,key);
	if(pos == -1) return pos;
	else
	{
		if(pos == REGS_POR_PAGINA-1) set_keys[pos].key = 0;
		else
		{
			for(i = pos; i < REGS_POR_PAGINA-1;i++)
			{
				set_keys[i].key = set_keys[i+1].key;
			}
			set_keys[i].key = 0;
		}
	}
	return 1;
}

_Bool removeRegistro(FILE *fi,FILE *ft,int key)
{
	int i,offset,busca,page;
	IndexNode inode;
	TerminalNode tnode;

	page = fread(&inode,sizeof(IndexNode),1,fi);

	if(page == 1)
	{
		while(!inode.leaf)
		{
			for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
			{
				if(key <= inode.keys[i]) break;
			}
			offset = sizeof(IndexNode) * (inode.pointers[i] - 1);
			fseek(fi,offset,SEEK_SET);
			fread(&inode,sizeof(IndexNode),1,fi);
		}

		for(i=0;i < 2*ORDEM && inode.keys[i] != 0;i++)
		{
			if(key <= inode.keys[i]) break;
		}

		if(inode.pointers[i] != 0)
		{
			offset = sizeof(TerminalNode) * ((inode.pointers[i] * -1)-1);
			fseek(ft,offset,SEEK_SET);
			fread(&tnode,sizeof(TerminalNode),1,ft);
			busca = removeOrdenadoTnode(tnode.reg,key);

			//Chave não encontrada
			if(busca == -1) return false;

			else
			{
				//Atualiza o nó podendo ficar vazio
				tnode.qtdeReg -=1;
				offset = sizeof(TerminalNode) * ((inode.pointers[i] * -1) -1);
				fseek(ft,offset,SEEK_SET);
				fwrite(&tnode,sizeof(TerminalNode),1,ft);

				//O terminal ficou vazio
				if(tnode.qtdeReg == 0)
				{
					//Posso remover do nó tranquilamente
					if(inode.qtdeKeys >= 2*ORDEM)
					{
						removeOrdenadoInode(&inode,i);
						offset = sizeof(IndexNode) * (inode.idNode - 1);
						fseek(fi,offset,SEEK_SET);
						fwrite(&inode,sizeof(IndexNode),1,fi);
					}
				}
			}
			return true;
		}
	}
	return false;
}

/*____________ MAIN ____________*/

int main()
{
	int key,age;
	char name[MAXSTR],operacao;
	FILE *fIndex,*fTerminal;
	Register reg,*cons;
	float start,end,timeElapsed;

	fIndex = checkFileIndex();
	fTerminal = checkFileTerminal();
	do
	{
		fseek(fIndex,0,SEEK_SET);
		fseek(fTerminal,0,SEEK_SET);
		scanf("%c%*c",&operacao);
		operacao = tolower(operacao);
		switch(operacao)
		{
			//Sair do Programa
			case 'e':
			{
				fclose(fIndex);
				fclose(fTerminal);
				exit(1);
				break;
			}

			//Insere Registro
			case 'i':
			{
				scanf("%d%*c",&key);
				getstr(name,MAXSTR);
				scanf("%d%*c",&age);
				if(fIndex && fTerminal)
				{
					reg.key = key;
					strcpy(reg.name,name);
					reg.age = age;
					if(!insereRegistro(fIndex,fTerminal,&reg))
					{
						printf("chave ja existente: %d\n",key);
					}
				}
				break;
			}

			//Consulta Regisro
			case 'c':
			{
				scanf("%d%*c",&key);
				start = (float)clock()/CLOCKS_PER_SEC;
				cons = consultaRegistro(fIndex,fTerminal,key);
				end = (float)clock()/CLOCKS_PER_SEC;
				timeElapsed = end - start;
				if(cons) printf("chave: %d\n%s\n%d\n",key,cons->name,cons->age);
				else printf("chave nao encontrada\n");

				printf("query rapida: %f segundos\n",timeElapsed);
				start = (float)clock()/CLOCKS_PER_SEC;
				cons = slowQuery(fTerminal,key);
				end = (float)clock()/CLOCKS_PER_SEC;
				timeElapsed = end - start;
				printf("query lenta: %f segundos\n",timeElapsed);
				break;
			}

			//Remove Registro
			case 'r':
			{
				scanf("%d%*c",&key);
				if(!removeRegistro(fIndex,fTerminal,key))
				{
					 printf("chave nao encontrada\n");
				}
				break;
			}

			//Imprime Árvore
			case 'p':
			{
				imprimeArvore(fIndex,fTerminal);
				break;
			}

			//Imprime nós Terminais
			case 'f':
			{
				imprimeNosTerminais(fTerminal);
				break;
			}
		}

	}while(operacao != 'e');

	return 0;
}
