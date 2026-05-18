#include <linux/module.h>
#include <linux/usb.h>
#include <linux/slab.h>

MODULE_AUTHOR("DevTITANS <devtitans@icomp.ufam.edu.br>");
MODULE_DESCRIPTION("Driver de acesso ao SmartLamp (ESP32 com Chip Serial CP2102)");
MODULE_LICENSE("GPL");


#define MAX_RECV_LINE 100 // Tamanho máximo de uma linha de resposta do dispositivo USB

static struct usb_device *smartlamp_device;        // Referência para o dispositivo USB
static uint usb_in, usb_out;                       // Endereços das portas de entrada e saida da USB
static char *usb_in_buffer, *usb_out_buffer;       // Buffers de entrada e saída da USB
static int usb_max_size;                           // Tamanho máximo de uma mensagem USB

// AVISO: Para que este driver possa executar sobre a porta USB do ESP32, 
// é necessário colocar um dos seguintes drivers no /etc/modprobe.d/blacklist.conf,
// dependendo do modelo de ESP32 utilzado:
//  -> ESP32 CP2102:
//      "blacklist cp210x"
//  -> ESP32 CH340:
//      "blacklist cdc_acm"

#define VENDOR_CP2102  0x10C4
#define PRODUCT_CP2102 0xEA60

#define VENDOR_CH340   0x1a86
#define PRODUCT_CH340  0x55d4

static const struct usb_device_id id_table[] = { 
    { USB_DEVICE(VENDOR_CP2102, PRODUCT_CP2102) }, 
    { USB_DEVICE(VENDOR_CH340, PRODUCT_CH340) }, 
    {} /* Fim da lista de ESP32s */
};

// Estrutura padrão CDC para configuração de porta serial
struct cdc_line_coding {
    __le32 dwDTERate;   // Baud rate
    __u8   bCharFormat; // Stop bits (0 = 1 stop bit)
    __u8   bParityType; // Paridade (0 = none)
    __u8   bDataBits;   // Data bits (8)
} __attribute__((packed));

static int  usb_probe(struct usb_interface *ifce, const struct usb_device_id *id); // Executado quando o dispositivo é conectado na USB
static void usb_disconnect(struct usb_interface *ifce);                            // Executado quando o dispositivo USB é desconectado da USB
static int  usb_write_serial(char *cmd, int param);                                // Executado para enviar um comando para o dispositivo

// Função para configurar os parâmetros seriais do CP2102 e do CH340 via Control-Messages
static int smartlamp_config_serial(struct usb_device *dev)
{
    int ret;
    u16 vendor = le16_to_cpu(dev->descriptor.idVendor);

    if (vendor == VENDOR_CP2102) {
        // ==========================================
        // Lógica para o CP2102 (Silicon Labs)
        // ==========================================
        u32 *baudrate;
        printk(KERN_INFO "SmartLamp: Configurando CP2102...\n");

        // 1. Habilita UART
        ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
                              0x00, 0x41, 0x0001, 0, NULL, 0, 1000);

        if (ret < 0) {
            return ret;
        }

        // 2. Define Baud Rate
        baudrate = kmalloc(sizeof(u32), GFP_KERNEL);
        if (!baudrate) return -ENOMEM;
        
        *baudrate = 9600;

        ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
                              0x1E, 0x41, 0, 0, baudrate, sizeof(u32), 1000);

        kfree(baudrate);

        if (ret < 0) {
            return ret;
        }

        printk(KERN_INFO "SmartLamp: Baud rate CP2102 configurado.\n");
        return 0;

    } else if (vendor == VENDOR_CH340) {
        // ==========================================
        // Lógica para o CH340 (CDC ACM)
        // ==========================================
        struct cdc_line_coding *line_coding;
        printk(KERN_INFO "SmartLamp: Configurando CH340 (CDC ACM)...\n");

        line_coding = kmalloc(sizeof(struct cdc_line_coding), GFP_KERNEL);
        if (!line_coding) return -ENOMEM;

        line_coding->dwDTERate = cpu_to_le32(9600);
        line_coding->bCharFormat = 0; 
        line_coding->bParityType = 0;
        line_coding->bDataBits = 8;

        ret = usb_control_msg(dev, usb_sndctrlpipe(dev, 0),
                              0x20, 0x21, 0, 0, 
                              line_coding, sizeof(struct cdc_line_coding), 1000);
        
        kfree(line_coding);
        if (ret < 0) {
            return ret;
        }
        
        printk(KERN_INFO "SmartLamp: Baud rate CH340 configurado.\n");
        return 0;
    }

    printk(KERN_ERR "SmartLamp: Dispositivo desconhecido.\n");
    return -ENODEV;
}


MODULE_DEVICE_TABLE(usb, id_table);

static struct usb_driver smartlamp_driver = {
    .name        = "smartlamp",     // Nome do driver
    .probe       = usb_probe,       // Executado quando o dispositivo é conectado na USB
    .disconnect  = usb_disconnect,  // Executado quando o dispositivo é desconectado na USB
    .id_table    = id_table,        // Tabela com o VendorID e ProductID do dispositivo
};

module_usb_driver(smartlamp_driver);

// Executado quando o dispositivo é conectado na USB
static int usb_probe(struct usb_interface *interface, const struct usb_device_id *id) {
    struct usb_endpoint_descriptor *usb_endpoint_in, *usb_endpoint_out;
    int err, ret;
    u16 vendor;

    smartlamp_device = interface_to_usbdev(interface);
    vendor = le16_to_cpu(smartlamp_device->descriptor.idVendor);

    // Regra exclusiva para o CH340: Ignorar a interface de controle (0)
    if (vendor == VENDOR_CH340 && interface->cur_altsetting->desc.bInterfaceNumber != 1) {
        printk(KERN_INFO "SmartLamp: Ignorando interface de controle do CH340\n");
        return -ENODEV; 
    }

    printk(KERN_INFO "SmartLamp: Dispositivo conectado ...\n");

    // 1. Procura os endpoints com segurança
    err = usb_find_common_endpoints(interface->cur_altsetting, &usb_endpoint_in, &usb_endpoint_out, NULL, NULL);
    if (err) {
        printk(KERN_ERR "SmartLamp: Endpoints IN/OUT nao encontrados (erro %d)\n", err);
        return err; // Impede o Kernel Panic
    }

    // 2. Aloca buffers
    usb_max_size = usb_endpoint_maxp(usb_endpoint_in);
    usb_in = usb_endpoint_in->bEndpointAddress;
    usb_out = usb_endpoint_out->bEndpointAddress;
    
    usb_in_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    if (!usb_in_buffer) return -ENOMEM;
    
    usb_out_buffer = kmalloc(usb_max_size, GFP_KERNEL);
    if (!usb_out_buffer) {
        kfree(usb_in_buffer);
        return -ENOMEM;
    }

    // 3. Configura a porta serial chamando a função smartlamp_config_serial()
    ret = smartlamp_config_serial(smartlamp_device);
    if (ret) {
        printk(KERN_ERR "SmartLamp: Falha na configuracao da serial\n");
        kfree(usb_in_buffer);
        kfree(usb_out_buffer);
        return ret;
    }

    printk(KERN_INFO "SmartLamp: Driver inicializado com sucesso!\n");

    // TASK 2.2: Chame a função usb_write_serial para enviar o comando SET_LED com valor 100
    // Descomente a linha abaixo e implemente a função usb_write_serial
    ret = usb_write_serial("SET_TRESHOLD", 1);
    ret = usb_write_serial("SET_LED", 100);
    
    return 0;
}

// Executado quando o dispositivo USB é desconectado da USB
static void usb_disconnect(struct usb_interface *interface) {
    printk(KERN_INFO "SmartLamp: Dispositivo desconectado.\n");
    kfree(usb_in_buffer);                   // Desaloca buffers
    kfree(usb_out_buffer);
}

// Envia um comando para o dispositivo USB
// Exemplo de uso: usb_write_serial("SET_LED", 80);
// Exemplo de uso: usb_write_serial("GET_LDR", 0);
static int usb_write_serial(char *cmd, int param) {
    int ret, actual_size;

    printk(KERN_INFO "SmartLamp: Enviando comando: %s %d\n", cmd, param);

    // TASK 2.2: Implemente o envio do comando para o dispositivo
    // Dica: Formate o comando no buffer usb_out_buffer e envie usando usb_bulk_msg
    // O formato esperado é: "COMANDO PARAMETRO\n"

    // Formata o comando no buffer de saída adicionando o \n exigido pelo protocolo
    sprintf(usb_out_buffer, "%s %d\n", cmd, param);

    // Envia o comando pela porta serial da USB
    ret = usb_bulk_msg(smartlamp_device, usb_sndbulkpipe(smartlamp_device, usb_out),
                       usb_out_buffer, strlen(usb_out_buffer), &actual_size, 1000);
    
    if (ret) {
        printk(KERN_ERR "SmartLamp: Erro ao enviar comando (código %d)\n", ret);
        return -1;
    }

    return 0;
}
