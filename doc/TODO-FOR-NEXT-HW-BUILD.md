1. Change device tree in fw/boards/nexrx/nexrx.dts to move two signals to new pins:

		/* Update GNSS reset GPIO from gpiob 3 to gpioc 6 */
		gnss_reset: gnss_reset {
			compatible = "nexrx,gnss-reset";
			reset-gpios = <&gpioc 6 GPIO_ACTIVE_LOW>;
			status = "okay";
		};

		/* Add USB PHY reset GPIO on gpioc 4 */
		usb_reset: usb_reset {
			compatible = "nexrx,usb-reset";
			reset-gpios = <&gpioc 4 GPIO_ACTIVE_LOW>;
			status = "okay";
		};
