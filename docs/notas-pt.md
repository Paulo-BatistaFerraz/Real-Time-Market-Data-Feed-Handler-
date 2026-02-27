# Notas de Estudo — Market Data Feed Handler (Português)

## O Projeto

Dois executáveis que se comunicam pela rede:
- **Exchange Simulator** — finge ser a bolsa de valores, gera eventos de mercado
- **Feed Handler** — recebe os dados e reconstrói o livro de ordens em tempo real

---

## Conceitos Aprendidos

### `#pragma pack(push, 1)` — Structs Empacotadas

**Problema:** O compilador insere bytes invisíveis (padding) entre os campos de um struct para alinhar a memória. Isso faz a CPU ler mais rápido, mas bagunça os bytes quando mandamos pela rede.

**Analogia da caixa organizadora:**
- Sem `pack` → cada objeto vai numa divisória separada, com espaço vazio entre eles
- Com `pack(push, 1)` → tudo enfiado junto, sem espaço

**O que faz:**
- `push` = salva a configuração atual do compilador numa pilha
- `1` = muda o alinhamento pra 1 byte (sem padding)
- `pop` = restaura a configuração anterior

**Por que usamos:** Mandamos structs como bytes crus pela rede. Os dois lados (simulador e feed handler) precisam concordar exatamente onde cada campo começa e termina. Padding invisível quebraria isso.

**Resumo:** `pack` sacrifica um pouco de velocidade da CPU pra garantir que os bytes ficam idênticos dos dois lados da rede.

```
Sem pack:  [ dado | ~~~ vazio ~~~ ][ caneta... ][ lápis | ~ vazio ~ ]  = 24 bytes
Com pack:  [ dado ][ caneta... ][ lápis ]                              = 13 bytes
```

---

### MessageHeader — Cabeçalho do Protocolo MiniITCH

Toda mensagem que viaja pela rede tem esse cabeçalho no início:

| Campo | Tipo | Tamanho | Descrição |
|---|---|---|---|
| `message_length` | `uint16_t` | 2 bytes | Tamanho total da mensagem incluindo o header |
| `message_type` | `char` | 1 byte | Tipo: 'A', 'X', 'E', 'R', 'T' |
| `timestamp` | `uint64_t` | 8 bytes | Nanossegundos desde meia-noite |

Total: 11 bytes (empacotado, sem padding)

---

### Tipos de Mensagem

| Tipo | Char | O que faz | Campos do Payload |
|---|---|---|---|
| AddOrder | 'A' | Nova ordem no livro | order_id, side, symbol, price, quantity |
| CancelOrder | 'X' | Remove uma ordem | order_id |
| ExecuteOrder | 'E' | Execução parcial/total | order_id, exec_quantity |
| ReplaceOrder | 'R' | Modifica preço/quantidade | order_id, new_price, new_quantity |
| TradeMessage | 'T' | Trade cruzado | symbol, price, quantity, buy_order_id, sell_order_id |

---

### Tipos Customizados (`types.hpp`)

| Tipo | Definição | Tamanho | Por quê? |
|---|---|---|---|
| `Price` | `uint32_t` | 4 bytes | Preço em ponto fixo (valor x 10000). Evita floating point. |
| `Quantity` | `uint32_t` | 4 bytes | Quantidade de ações |
| `OrderId` | `uint64_t` | 8 bytes | ID único da ordem, sempre crescendo |
| `Timestamp` | `uint64_t` | 8 bytes | Nanossegundos desde meia-noite |
| `Side` | `uint8_t` (enum) | 1 byte | Buy (0x01) ou Sell (0x02) |
| `Symbol` | `char[8]` (struct) | 8 bytes | Ticker como "AAPL", preenchido com zeros |

---

### Arquitetura do Feed Handler — 4 Threads

```
  Pacotes UDP chegam
         │
         ▼
  ┌──────────────┐
  │   NETWORK    │  Thread 1: recebe pacotes, marca o horário de chegada
  └──────┬───────┘
         │ Q1 (bytes crus)
         ▼
  ┌──────────────┐
  │   PARSER     │  Thread 2: lê binário, transforma bytes em structs
  └──────┬───────┘
         │ Q2 (mensagens tipadas)
         ▼
  ┌──────────────┐
  │  ORDER BOOK  │  Thread 3: aplica Add/Cancel/Execute no livro
  └──────┬───────┘
         │ Q3 (atualizações do livro)
         ▼
  ┌──────────────┐
  │  CONSUMER    │  Thread 4: mostra estatísticas, mede latência
  └──────────────┘
```

Cada thread só fala com a vizinha através de **filas SPSC lock-free** (sem mutex, sem trava).

---

### `#pragma once`

Previne que o mesmo header seja incluído duas vezes durante a compilação. Alternativa ao `#ifndef` / `#define` / `#endif` tradicional.

---

### Por que usar `OrderId` em vez de `uint64_t`?

**Resposta de entrevista:** Clareza semântica + ponto único de mudança.

- `uint64_t` te fala o **tamanho** (64 bits)
- `OrderId` te fala o **significado** (é um ID de ordem)
- Qualquer pessoa na empresa lê `OrderId` e sabe o que é, sem precisar de comentário
- Se precisar mudar o tipo, muda em **um lugar** (`types.hpp`), não em 40 arquivos

```cpp
// Ruim — o que é cada uint64_t?
void cancel(uint64_t id, uint32_t qty);

// Bom — se lê como inglês
void cancel(OrderId id, Quantity qty);
```

---

### `static constexpr` — Constantes dentro de structs

- `static` = pertence ao **tipo**, não a uma instância. Você acessa com `AddOrder::TYPE`, não precisa criar um objeto.
- `constexpr` = resolvido em **tempo de compilação**. O compilador substitui pelo valor direto, sem custo em runtime.

### `sizeof` dentro vs fora do struct

Não pode usar `sizeof(AddOrder)` **dentro** do próprio `AddOrder` — o struct ainda não terminou de ser definido (tipo incompleto). Solução: coloca o número na mão e usa `static_assert` **depois** do struct pra verificar:

```cpp
struct AddOrder {
    static constexpr uint16_t WIRE_SIZE = 36;  // hardcoded
    // ... campos ...
};
static_assert(sizeof(MessageHeader) + sizeof(AddOrder) == AddOrder::WIRE_SIZE,
    "AddOrder WIRE_SIZE mismatch");  // compilador verifica pra você
```

### `std::variant` — ParsedMessage

Uma "caixa" que pode conter **qualquer um** dos 5 tipos de mensagem, mas só um de cada vez. O parser thread usa isso pra passar mensagens pro próximo estágio sem saber antecipadamente qual tipo vai ser.

```cpp
using ParsedMessage = std::variant<AddOrder, CancelOrder, ExecuteOrder, ReplaceOrder, TradeMessage>;
```

### `enum class` vs `char` solto

- `char type = 'Q'` → compila, mas 'Q' não é tipo válido. Bug silencioso.
- `MessageType type = 'Q'` → **erro de compilação**. O compilador te protege.
- `: char` no final do enum diz que por baixo ocupa 1 byte, igual a um char.

---

*Última atualização: 2026-02-27 — Task 2 + início Task 3*
