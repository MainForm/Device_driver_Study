#define pr_fmt(fmt) "[" KBUILD_MODNAME "][%s]: " fmt, __func__

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/types.h>

#define I2C_DEVICE_TYPE		"i2c_test"
#define I2C_DEVICE_ADDR		0x10

static struct i2c_adapter *i2c_adapter;
static struct i2c_client *i2c_client;

#define COMMAND_GET_DEVICE_ID	0x01
#define COMMAND_SET_LED		0x02

#define LED_OFF			0x00
#define LED_ON			0x01

/**
 * send_command - I2C 장치에 명령을 전송하고 응답을 수신한다
 * @client: 명령을 전송할 I2C client
 * @command: 장치에 전달할 명령 코드
 * @argument: 명령과 함께 전달할 인자
 * @response: 장치가 반환한 1바이트 응답을 저장할 위치
 *
 * command와 argument를 차례로 전송한 후 장치의 응답 1바이트를 읽는다.
 *
 * Return: 성공하면 0, 실패하면 음수 오류 코드를 반환한다.
 */
static int send_command(struct i2c_client *client, u8 command, u8 argument,
				u8 *response)
{
	u8 tx_buf[] = { command, argument };
	int ret;

	/*
	 * I2C client로 2바이트 명령을 송신한다.
	 * 성공하면 실제로 송신한 바이트 수를, 실패하면 음수 오류 코드를
	 * 반환한다. 따라서 2바이트를 전부 송신했을 때만 정상으로 처리한다.
	 */
	ret = i2c_master_send(client, (const char *)tx_buf, sizeof(tx_buf));

	if (ret < 0) {
		pr_err("failed to send command: %d\n", ret);
		return ret;
	} else if (ret != sizeof(tx_buf)) {
		pr_err("incomplete command write: %d of %zu bytes\n",
		       ret, sizeof(tx_buf));
		return -EIO;
	}

	/*
	 * I2C client로부터 명령에 대한 응답 1바이트를 수신한다.
	 * 성공하면 실제로 수신한 바이트 수를, 실패하면 음수 오류 코드를
	 * 반환한다. 따라서 1바이트를 정상적으로 수신했을 때만 성공으로 처리한다.
	 */
	ret = i2c_master_recv(client, (char *)response, sizeof(*response));

	if (ret < 0) {
		pr_err("failed to receive response: %d\n", ret);
		return ret;
	} else if (ret != sizeof(*response)) {
		pr_err("incomplete response read: %d of %zu bytes\n",
		       ret, sizeof(*response));
		return -EIO;
	}

	return 0;
}

static int __init i2c_test_init(void)
{
	struct i2c_board_info board_info = {
		I2C_BOARD_INFO(I2C_DEVICE_TYPE, I2C_DEVICE_ADDR)
	};
	u8 command_response;
	u8 device_id;
	int ret_err;

	pr_info("---- init i2c device ----\n");

	pr_info("initializing I2C test device\n");

	/*
	 * I2C 버스 1을 나타내는 adapter를 가져온다.
	 * adapter를 찾지 못하면 NULL을 반환한다.
	 */
	pr_info("getting I2C adapter 1\n");
	i2c_adapter = i2c_get_adapter(1);
	
	if (!i2c_adapter) {
		pr_err("I2C adapter 1 not found\n");
		return -ENODEV;
	}

	/* adapter가 일반 I2C 메시지 전송을 지원하는지 확인한다. */
	pr_info("checking for plain I2C transfer support\n");
	if (!i2c_check_functionality(i2c_adapter, I2C_FUNC_I2C)) {
		pr_err("I2C adapter 1 does not support plain I2C transfers\n");
		ret_err = -EOPNOTSUPP;
		goto err_i2c_put_adapter;
	}

	/* adapter의 0x10 주소에 I2C client device를 등록한다. */
	pr_info("registering I2C device at address 0x%02x\n",
		I2C_DEVICE_ADDR);
	i2c_client = i2c_new_client_device(i2c_adapter, &board_info);
	if (IS_ERR(i2c_client)) {
		ret_err = PTR_ERR(i2c_client);
		pr_err("failed to register I2C device: %d\n", ret_err);
		i2c_client = NULL;
		goto err_i2c_put_adapter;
	}

	/*
	 * Device ID 명령을 전송하고 응답을 읽어
	 * I2C 장치와 정상적으로 통신할 수 있는지 확인한다.
	 */
	pr_info("reading device ID\n");
	ret_err = send_command(i2c_client, COMMAND_GET_DEVICE_ID, 0x00,
			       &device_id);
	if (ret_err < 0)
		goto err_i2c_unregister_device;

	pr_info("device ID: 0x%02x\n", device_id);

	/*
	 * LED 켜기 명령을 전송하고 응답을 받아
	 * Slave에서 Master와 연결되었음을 보여준다.
	 */
	pr_info("turning on the built-in LED\n");

	ret_err = send_command(i2c_client, COMMAND_SET_LED, LED_ON,
			       &command_response);
	if (ret_err < 0)
		goto err_i2c_unregister_device;

	pr_info("I2C test device initialized\n");

	return 0;

err_i2c_unregister_device:
	if (i2c_client)
		i2c_unregister_device(i2c_client);
err_i2c_put_adapter:
	if (i2c_adapter)
		i2c_put_adapter(i2c_adapter);

	return ret_err;
}

static void __exit i2c_test_exit(void)
{
	u8 response;
	
	pr_info("---- exit i2c device ----\n");
	pr_info("removing I2C test device\n");

	if (i2c_client) {
		send_command(i2c_client, COMMAND_SET_LED, LED_OFF, &response);
		i2c_unregister_device(i2c_client);
	}

	if (i2c_adapter)
		i2c_put_adapter(i2c_adapter);
}

module_init(i2c_test_init);
module_exit(i2c_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C peripheral test driver");
