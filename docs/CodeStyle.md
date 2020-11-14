Just a reference :P

- Class name: `DemoClass`

- Class member: `m_member_name`

- pointer: `name_p`, `object_p`


Example:

	class DemoClass
	{
		public:
			DemoClass();

			void init();

			void setValue( const int new_value);
			int getValue() const;

		private:
			int m_value;
			PropertyObject *m_property_p;
	};

	class CustomWidget : public QWidget
	{
		Q_OBJECT
		public:

			CustomWidget( QObject *parent = nullptr);

			void init();
	};
