/***************************************************************************
                          kdeinterface.h  -  description
                             -------------------
    begin                : Sun Mar 25 2001
    copyright            : (C) 2001 by andres
    email                : dae@chez.com
 ***************************************************************************/

#ifndef	_KDEINTERFACE_H_
#define	_KDEINTERFACE_H_

class KApplication;
class KInterfaceMain;
class KAboutData;

class KDEInterface
{
	private:
		KDEInterface ( KDEInterface &kdeInterface ) {};
		KDEInterface &operator= ( KDEInterface &kdeInterface ) { return ( *this ); };
		
	public:
		KDEInterface(intf_thread_t *p_intf);
		~KDEInterface();

		// These methods get exported to the core
		static int		probe	( probedata_t *p_data );
		static int		open	( intf_thread_t *p_intf );
		static void	close	( intf_thread_t *p_intf );
		static void	run		( intf_thread_t *p_intf );
	
	private:
		KApplication		*fApplication;
		KInterfaceMain	*fWindow;
		KAboutData		*fAboutData;

};

#endif /* _KDEINTERFACE_H_ */
